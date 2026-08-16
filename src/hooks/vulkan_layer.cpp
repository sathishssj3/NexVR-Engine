
#include "rendering/vulkan/vk_layer.h"
#include <atomic>
#include <mutex>
#include <iostream>
#include "rendering/stereo/stereo_pipeline.h"
#include "core/logger.h"
#include "hooks/vulkan_hook.h"
#include "rendering/vulkan/vulkan_dispatch_table.h"
#include "core/frame_coordinator.h"
#include "rendering/vulkan/vulkan_lifecycle_manager.h"
#include "rendering/vulkan/vulkan_queue_manager.h"
#include "rendering/vulkan/vulkan_snapshot_validator.h"
#include "rendering/vulkan/vulkan_depth_candidate_collector.h"

namespace vrinject {
namespace vulkan {
namespace hooks {

// Whether the Vulkan loader activated us as a layer. Read from the RuntimeState
// background thread (HookManager) and written from whichever thread the loader uses
// to negotiate the layer interface, so it must be atomic.
static std::atomic<bool> s_layerActive{false};

void SetLayerActive() {
    // Log once on the transition; this is called from several entry points because the
    // loader picks whichever interface the manifest advertises.
    if (!s_layerActive.exchange(true, std::memory_order_acq_rel)) {
        LOG_INFO("Vulkan Layer: active - MinHook Vulkan detours will be suppressed.");
    }
}
bool IsLayerActive()  { return s_layerActive.load(std::memory_order_acquire); }

} // namespace hooks
} // namespace vulkan
} // namespace vrinject


namespace vrinject {
    namespace DX11Hook {
        // extern StereoPipeline g_stereoPipeline; // VulkanRenderer is deprecated in favor of FrameCoordinator + VulkanGraphicsBackend
    }
}

static std::once_flag s_vkInitFlag;

#include <unordered_map>

namespace {
    /// Guards every dispatch map below. The maps are filled on whichever thread the loader
    /// uses for vkCreateInstance/vkCreateDevice and read from every render thread that calls
    /// a hooked entry point, so unsynchronised access is a data race on an unordered_map -
    /// an insert can rehash while another thread walks a bucket, which is the 0xC0000409
    /// heap corruption seen during swapchain creation.
    ///
    /// Lock ordering: g_dispatchMutex is a leaf. Never call a next-layer function while
    /// holding it (downstream layers re-enter our GetProcAddr entry points) and never take
    /// g_swapchainMutex under it.
    std::mutex g_dispatchMutex;

    // Dispatch tables for calling the next layer
    std::unordered_map<void*, PFN_vkGetInstanceProcAddr> g_instanceDispatch;
    std::unordered_map<void*, PFN_vkGetDeviceProcAddr> g_deviceDispatch;

    // Store original function pointers for the functions we hook
    std::unordered_map<void*, PFN_vkQueuePresentKHR> g_nextQueuePresent;
    std::unordered_map<void*, PFN_vkCreateSwapchainKHR> g_nextCreateSwapchain;
    std::unordered_map<void*, PFN_vkGetSwapchainImagesKHR> g_nextGetSwapchainImages;
    std::unordered_map<void*, PFN_vkMapMemory> g_nextMapMemory;
    std::unordered_map<void*, PFN_vkCmdUpdateBuffer> g_nextCmdUpdateBuffer;
    std::unordered_map<void*, PFN_vkGetDeviceQueue> g_nextGetDeviceQueue;

    // Store swapchain images so we can find the backbuffer on Present
    std::mutex g_swapchainMutex;
    std::unordered_map<VkSwapchainKHR, std::vector<VkImage>> g_swapchainImages;
    std::unordered_map<VkQueue, VkDevice> g_queueToDevice;
    std::atomic<VkInstance> g_vulkanInstance{VK_NULL_HANDLE};
    std::unordered_map<VkDevice, VkPhysicalDevice> g_deviceToPhysicalDevice;

    template<typename T>
    inline void* GetDispatchKey(T object) {
        return (void*)*(void**)object;
    }

    /// @brief Copy a next-layer pointer out of a dispatch map under g_dispatchMutex.
    /// @return The stored pointer, or nullptr when the object was never tracked (late
    ///         injection, or a device created before the layer was inserted).
    /// @note Uses find(), not operator[] - operator[] inserts on a miss, so it mutates the
    ///       map on what reads like a read and cannot be made safe by a reader-only lock.
    template<typename Map, typename Key>
    inline typename Map::mapped_type LookupNext(const Map& map, Key key) {
        std::lock_guard<std::mutex> lock(g_dispatchMutex);
        auto it = map.find(key);
        return it != map.end() ? it->second : nullptr;
    }
}

VKAPI_ATTR VkResult VKAPI_CALL VRInject_vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance) {
    vrinject::vulkan::hooks::SetLayerActive();

    VkLayerInstanceCreateInfo* layerCreateInfo = (VkLayerInstanceCreateInfo*)pCreateInfo->pNext;
    while (layerCreateInfo && (layerCreateInfo->sType != VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO || layerCreateInfo->function != VK_LAYER_LINK_INFO)) {
        layerCreateInfo = (VkLayerInstanceCreateInfo*)layerCreateInfo->pNext;
    }

    if (layerCreateInfo == nullptr) return VK_ERROR_INITIALIZATION_FAILED;

    PFN_vkGetInstanceProcAddr gpa = layerCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    layerCreateInfo->u.pLayerInfo = layerCreateInfo->u.pLayerInfo->pNext;

    PFN_vkCreateInstance createFunc = (PFN_vkCreateInstance)gpa(VK_NULL_HANDLE, "vkCreateInstance");
    VkResult res = createFunc(pCreateInfo, pAllocator, pInstance);

    if (res == VK_SUCCESS) {
        void* key = GetDispatchKey(*pInstance);
        {
            std::lock_guard<std::mutex> lock(g_dispatchMutex);
            g_instanceDispatch[key] = gpa;
        }
        g_vulkanInstance.store(*pInstance, std::memory_order_release);
        
        vrinject::vulkan::VulkanDispatchTable::Get().InitOriginalGetInstanceProcAddr(gpa);
        vrinject::vulkan::VulkanDispatchTable::Get().RegisterInstance(*pInstance);

        uint32_t apiVersion = pCreateInfo && pCreateInfo->pApplicationInfo ? pCreateInfo->pApplicationInfo->apiVersion : 0;
        vrinject::vulkan::VulkanLifecycleManager::Get().OnInstanceCreated(*pInstance, apiVersion);

        LOG_INFO("Vulkan Layer: vkCreateInstance intercepted");
    }
    return res;
}

VKAPI_ATTR VkResult VKAPI_CALL VRInject_vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) {
    VkLayerDeviceCreateInfo* layerCreateInfo = (VkLayerDeviceCreateInfo*)pCreateInfo->pNext;
    while (layerCreateInfo && (layerCreateInfo->sType != VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO || layerCreateInfo->function != VK_LAYER_LINK_INFO)) {
        layerCreateInfo = (VkLayerDeviceCreateInfo*)layerCreateInfo->pNext;
    }

    if (layerCreateInfo == nullptr) return VK_ERROR_INITIALIZATION_FAILED;

    PFN_vkGetInstanceProcAddr gpa = layerCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr gdpa = layerCreateInfo->u.pLayerInfo->pfnNextGetDeviceProcAddr;
    layerCreateInfo->u.pLayerInfo = layerCreateInfo->u.pLayerInfo->pNext;

    PFN_vkCreateDevice createFunc = (PFN_vkCreateDevice)gpa((VkInstance)physicalDevice, "vkCreateDevice");
    VkResult res = createFunc(physicalDevice, pCreateInfo, pAllocator, pDevice);

    if (res == VK_SUCCESS) {
        void* key = GetDispatchKey(*pDevice);

        // Resolve every next-layer pointer BEFORE taking the lock: gdpa() re-enters the
        // layer chain below us, and holding g_dispatchMutex across that risks deadlock.
        auto nextQueuePresent      = (PFN_vkQueuePresentKHR)gdpa(*pDevice, "vkQueuePresentKHR");
        auto nextCreateSwapchain   = (PFN_vkCreateSwapchainKHR)gdpa(*pDevice, "vkCreateSwapchainKHR");
        auto nextGetSwapchainImgs  = (PFN_vkGetSwapchainImagesKHR)gdpa(*pDevice, "vkGetSwapchainImagesKHR");
        auto nextMapMemory         = (PFN_vkMapMemory)gdpa(*pDevice, "vkMapMemory");
        auto nextCmdUpdateBuffer   = (PFN_vkCmdUpdateBuffer)gdpa(*pDevice, "vkCmdUpdateBuffer");
        auto nextGetDeviceQueue    = (PFN_vkGetDeviceQueue)gdpa(*pDevice, "vkGetDeviceQueue");

        {
            std::lock_guard<std::mutex> lock(g_dispatchMutex);
            g_deviceDispatch[key]           = gdpa;
            g_nextQueuePresent[key]         = nextQueuePresent;
            g_nextCreateSwapchain[key]      = nextCreateSwapchain;
            g_nextGetSwapchainImages[key]   = nextGetSwapchainImgs;
            g_nextMapMemory[key]            = nextMapMemory;
            g_nextCmdUpdateBuffer[key]      = nextCmdUpdateBuffer;
            g_nextGetDeviceQueue[key]       = nextGetDeviceQueue;
        }

        // g_deviceToPhysicalDevice is read under g_swapchainMutex in QueuePresent, so it is
        // written under the same lock. Taken after g_dispatchMutex is released, never nested.
        {
            std::lock_guard<std::mutex> lock(g_swapchainMutex);
            g_deviceToPhysicalDevice[*pDevice] = physicalDevice;
        }

        vrinject::vulkan::VulkanDispatchTable::Get().InitOriginalGetDeviceProcAddr(gdpa);
        vrinject::vulkan::VulkanDispatchTable::Get().RegisterDevice(*pDevice, g_vulkanInstance.load(std::memory_order_acquire));

        vrinject::vulkan::VulkanLifecycleManager::Get().OnDeviceCreated(physicalDevice, *pDevice, pCreateInfo);

        LOG_INFO("Vulkan Layer: vkCreateDevice intercepted");
    }
    return res;
}

VKAPI_ATTR VkResult VKAPI_CALL VRInject_vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain) {
    auto nextFunc = LookupNext(g_nextCreateSwapchain, GetDispatchKey(device));
    if (!nextFunc) {
        // Untracked device - we cannot forward, and claiming VK_SUCCESS would hand the app
        // an uninitialised handle it would then use.
        LOG_WARN("Vulkan Layer: vkCreateSwapchainKHR on an untracked device - failing the call.");
        if (pSwapchain) *pSwapchain = VK_NULL_HANDLE;
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    VkResult res = nextFunc(device, pCreateInfo, pAllocator, pSwapchain);
    if (res == VK_SUCCESS && pSwapchain && *pSwapchain) {
        vrinject::vulkan::VulkanLifecycleManager::Get().OnSwapchainCreated(device, *pSwapchain, pCreateInfo);
    }
    return res;
}

VKAPI_ATTR VkResult VKAPI_CALL VRInject_vkGetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain, uint32_t* pSwapchainImageCount, VkImage* pSwapchainImages) {
    auto nextFunc = LookupNext(g_nextGetSwapchainImages, GetDispatchKey(device));
    if (!nextFunc) {
        LOG_WARN("Vulkan Layer: vkGetSwapchainImagesKHR on an untracked device - failing the call.");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    VkResult result = nextFunc(device, swapchain, pSwapchainImageCount, pSwapchainImages);

    if (result == VK_SUCCESS && pSwapchainImages != nullptr && pSwapchainImageCount != nullptr) {
        std::lock_guard<std::mutex> lock(g_swapchainMutex);
        auto& images = g_swapchainImages[swapchain];
        images.resize(*pSwapchainImageCount);
        for (uint32_t i = 0; i < *pSwapchainImageCount; ++i) {
            images[i] = pSwapchainImages[i];
        }
        LOG_INFO("Vulkan Layer: Captured %u swapchain images", *pSwapchainImageCount);
    }
    return result;
}

VKAPI_ATTR void VKAPI_CALL VRInject_vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue) {
    auto nextFunc = LookupNext(g_nextGetDeviceQueue, GetDispatchKey(device));
    if (nextFunc) {
        nextFunc(device, queueFamilyIndex, queueIndex, pQueue);
        if (pQueue && *pQueue) {
            std::lock_guard<std::mutex> lock(g_swapchainMutex);
            g_queueToDevice[*pQueue] = device;
        }
    }
}

VKAPI_ATTR VkResult VKAPI_CALL VRInject_QueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
    // Resolve the queue's device before arming the one-shot init. Doing the lookup inside
    // call_once would burn the flag on an untracked queue and initialise the renderer with
    // a null device, leaving it permanently broken for the queue we actually care about.
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    {
        std::lock_guard<std::mutex> lock(g_swapchainMutex);
        auto it = g_queueToDevice.find(queue);
        if (it != g_queueToDevice.end()) {
            device = it->second;
            auto physIt = g_deviceToPhysicalDevice.find(device);
            if (physIt != g_deviceToPhysicalDevice.end()) {
                physicalDevice = physIt->second;
            }
        }
    }

    if (device != VK_NULL_HANDLE) {
        vrinject::vulkan::VulkanQueueManager::Get().RegisterQueue(device, 0, 0, queue);
        
        vrinject::RenderFrameSnapshot snapshot = vrinject::vulkan::VulkanLifecycleManager::Get().CreateSnapshot(queue);
        std::string error;
        if (vrinject::vulkan::VulkanSnapshotValidator::Validate(snapshot, error)) {
            vrinject::FrameCoordinator::Get().OnPresentBegin(snapshot);
            vrinject::FrameCoordinator::Get().OnPresentEnd();
        } else {
            static int logCount = 0;
            if (logCount < 50) {
                LOG_WARN("Vulkan Layer: Snapshot validation failed: %s", error.c_str());
                logCount++;
            }
        }
        vrinject::vulkan::VulkanDepthCandidateCollector::Get().CollectCandidates(device);
    }

    auto nextFunc = LookupNext(g_nextQueuePresent, GetDispatchKey(queue));
    if (!nextFunc) {
        LOG_WARN("Vulkan Layer: vkQueuePresentKHR on an untracked queue - failing the call.");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return nextFunc(queue, pPresentInfo);
}

VKAPI_ATTR VkResult VKAPI_CALL VRInject_vkMapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData) {
    auto nextFunc = LookupNext(g_nextMapMemory, GetDispatchKey(device));
    if (!nextFunc) {
        LOG_WARN("Vulkan Layer: vkMapMemory on an untracked device - failing the call.");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    VkResult result = nextFunc(device, memory, offset, size, flags, ppData);

    if (result == VK_SUCCESS && ppData && *ppData) {
        // UniversalScanner analysis will go here
    }
    return result;
}

VKAPI_ATTR void VKAPI_CALL VRInject_vkCmdUpdateBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize dataSize, const void* pData) {
    auto nextFunc = LookupNext(g_nextCmdUpdateBuffer, GetDispatchKey(commandBuffer));
    if (nextFunc) nextFunc(commandBuffer, dstBuffer, dstOffset, dataSize, pData);
}

extern "C" {
    __declspec(dllexport) VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL VRInject_vkGetDeviceProcAddr(VkDevice device, const char* pName) {
        if (strcmp(pName, "vkQueuePresentKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(VRInject_QueuePresentKHR);
        if (strcmp(pName, "vkCreateSwapchainKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(VRInject_vkCreateSwapchainKHR);
        if (strcmp(pName, "vkGetSwapchainImagesKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(VRInject_vkGetSwapchainImagesKHR);
        if (strcmp(pName, "vkMapMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(VRInject_vkMapMemory);
        if (strcmp(pName, "vkCmdUpdateBuffer") == 0) return reinterpret_cast<PFN_vkVoidFunction>(VRInject_vkCmdUpdateBuffer);
        if (strcmp(pName, "vkCreateDevice") == 0) return reinterpret_cast<PFN_vkVoidFunction>(VRInject_vkCreateDevice);
        if (strcmp(pName, "vkGetDeviceQueue") == 0) return reinterpret_cast<PFN_vkVoidFunction>(VRInject_vkGetDeviceQueue);

        if (device == VK_NULL_HANDLE) return nullptr;

        // Untracked device: pure pass-through down the chain. Never synthesise a hooked
        // pointer here - that recurses straight back into us.
        auto nextGdpa = LookupNext(g_deviceDispatch, GetDispatchKey(device));
        return nextGdpa ? nextGdpa(device, pName) : nullptr;
    }

    __declspec(dllexport) VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL VRInject_vkGetInstanceProcAddr(VkInstance instance, const char* pName) {
        // Earliest entry point the loader actually calls when the manifest names the
        // legacy functions directly (it then never calls the negotiate export at all).
        vrinject::vulkan::hooks::SetLayerActive();

        if (strcmp(pName, "vkCreateInstance") == 0) return reinterpret_cast<PFN_vkVoidFunction>(VRInject_vkCreateInstance);
        if (strcmp(pName, "vkCreateDevice") == 0) return reinterpret_cast<PFN_vkVoidFunction>(VRInject_vkCreateDevice);
        
        // Instance-level functions
        if (strcmp(pName, "vkQueuePresentKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(VRInject_QueuePresentKHR);
        if (strcmp(pName, "vkCreateSwapchainKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(VRInject_vkCreateSwapchainKHR);
        if (strcmp(pName, "vkGetSwapchainImagesKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(VRInject_vkGetSwapchainImagesKHR);
        if (strcmp(pName, "vkMapMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(VRInject_vkMapMemory);
        if (strcmp(pName, "vkCmdUpdateBuffer") == 0) return reinterpret_cast<PFN_vkVoidFunction>(VRInject_vkCmdUpdateBuffer);

        // The loader queries global entry points (vkEnumerateInstanceExtensionProperties,
        // vkEnumerateInstanceLayerProperties, ...) with a null instance. GetDispatchKey
        // dereferences its argument, so it must not run on VK_NULL_HANDLE.
        if (instance == VK_NULL_HANDLE) return nullptr;

        auto nextGipa = LookupNext(g_instanceDispatch, GetDispatchKey(instance));
        return nextGipa ? nextGipa(instance, pName) : nullptr;
    }

    __declspec(dllexport) VKAPI_ATTR VkResult VKAPI_CALL VRInject_vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface* pVersionStruct) {
        // Earliest possible signal that we are in the loader's dispatch chain. HookManager
        // consults this before installing MinHook detours on vulkan-1.dll's exports; running
        // both interception strategies at once faults inside the loader (0xC0000409) during
        // swapchain creation.
        vrinject::vulkan::hooks::SetLayerActive();

        if (pVersionStruct->loaderLayerInterfaceVersion >= 2) {
            pVersionStruct->pfnGetInstanceProcAddr = VRInject_vkGetInstanceProcAddr;
            pVersionStruct->pfnGetDeviceProcAddr = VRInject_vkGetDeviceProcAddr;
            pVersionStruct->pfnGetPhysicalDeviceProcAddr = nullptr;
        }
        return VK_SUCCESS;
    }
}
