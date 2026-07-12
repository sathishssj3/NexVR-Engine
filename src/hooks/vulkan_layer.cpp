
#include "../rendering/vulkan/vk_layer.h"
#include <mutex>
#include <iostream>
#include "../rendering/backends/vulkan_renderer.h"
#include "../rendering/stereo_pipeline.h"
#include "../core/logger.h"


namespace vrinject {
    namespace DX11Hook {
        extern StereoPipeline g_stereoPipeline;
    }
}
vrinject::VulkanRenderer g_vkRenderer;

static std::once_flag s_vkInitFlag;

#include <unordered_map>

namespace {
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
    VkInstance g_vulkanInstance = VK_NULL_HANDLE;
    std::unordered_map<VkDevice, VkPhysicalDevice> g_deviceToPhysicalDevice;

    template<typename T>
    inline void* GetDispatchKey(T object) {
        return (void*)*(void**)object;
    }
}

VKAPI_ATTR VkResult VKAPI_CALL VRInject_vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance) {
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
        g_instanceDispatch[key] = gpa;
        g_vulkanInstance = *pInstance;
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
        g_deviceDispatch[key] = gdpa;
        
        g_deviceToPhysicalDevice[*pDevice] = physicalDevice;
        
        g_nextQueuePresent[key] = (PFN_vkQueuePresentKHR)gdpa(*pDevice, "vkQueuePresentKHR");
        g_nextCreateSwapchain[key] = (PFN_vkCreateSwapchainKHR)gdpa(*pDevice, "vkCreateSwapchainKHR");
        g_nextGetSwapchainImages[key] = (PFN_vkGetSwapchainImagesKHR)gdpa(*pDevice, "vkGetSwapchainImagesKHR");
        g_nextMapMemory[key] = (PFN_vkMapMemory)gdpa(*pDevice, "vkMapMemory");
        g_nextCmdUpdateBuffer[key] = (PFN_vkCmdUpdateBuffer)gdpa(*pDevice, "vkCmdUpdateBuffer");
        g_nextGetDeviceQueue[key] = (PFN_vkGetDeviceQueue)gdpa(*pDevice, "vkGetDeviceQueue");
        LOG_INFO("Vulkan Layer: vkCreateDevice intercepted");
    }
    return res;
}

VKAPI_ATTR VkResult VKAPI_CALL VRInject_vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain) {
    void* key = GetDispatchKey(device);
    auto nextFunc = g_nextCreateSwapchain[key];
    if (nextFunc) return nextFunc(device, pCreateInfo, pAllocator, pSwapchain);
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL VRInject_vkGetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain, uint32_t* pSwapchainImageCount, VkImage* pSwapchainImages) {
    void* key = GetDispatchKey(device);
    auto nextFunc = g_nextGetSwapchainImages[key];
    VkResult result = VK_SUCCESS;
    if (nextFunc) result = nextFunc(device, swapchain, pSwapchainImageCount, pSwapchainImages);

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
    void* key = GetDispatchKey(device);
    auto nextFunc = g_nextGetDeviceQueue[key];
    if (nextFunc) {
        nextFunc(device, queueFamilyIndex, queueIndex, pQueue);
        if (pQueue && *pQueue) {
            std::lock_guard<std::mutex> lock(g_swapchainMutex);
            g_queueToDevice[*pQueue] = device;
        }
    }
}

VKAPI_ATTR VkResult VKAPI_CALL VRInject_QueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
    std::call_once(s_vkInitFlag, [&]() {
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
        g_vkRenderer.SetVulkanContext(g_vulkanInstance, physicalDevice);
        g_vkRenderer.Initialize(device, queue);
        vrinject::DX11Hook::g_stereoPipeline.GetOpenXRManager()->SetRenderer(&g_vkRenderer);
        LOG_INFO("Vulkan backend initialized via IRenderer (Implicit Layer)");
    });

    if (pPresentInfo && pPresentInfo->swapchainCount > 0) {
        VkSwapchainKHR swapchain = pPresentInfo->pSwapchains[0];
        uint32_t imageIndex = pPresentInfo->pImageIndices[0];
        
        VkImage backbuffer = VK_NULL_HANDLE;
        {
            std::lock_guard<std::mutex> lock(g_swapchainMutex);
            auto it = g_swapchainImages.find(swapchain);
            if (it != g_swapchainImages.end() && imageIndex < it->second.size()) {
                backbuffer = it->second[imageIndex];
            }
        }
        
        if (backbuffer != VK_NULL_HANDLE) {
            // We now have the Vulkan backbuffer! 
            // In a real implementation, we would now transition this image and run stereoscopic split.
        }
    }

    void* key = GetDispatchKey(queue);
    auto nextFunc = g_nextQueuePresent[key];
    if (nextFunc) return nextFunc(queue, pPresentInfo);
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL VRInject_vkMapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData) {
    void* key = GetDispatchKey(device);
    auto nextFunc = g_nextMapMemory[key];
    VkResult result = VK_SUCCESS;
    if (nextFunc) result = nextFunc(device, memory, offset, size, flags, ppData);

    if (result == VK_SUCCESS && ppData && *ppData) {
        // UniversalScanner analysis will go here
    }
    return result;
}

VKAPI_ATTR void VKAPI_CALL VRInject_vkCmdUpdateBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize dataSize, const void* pData) {
    void* key = GetDispatchKey(commandBuffer);
    auto nextFunc = g_nextCmdUpdateBuffer[key];
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

        void* key = GetDispatchKey(device);
        auto it = g_deviceDispatch.find(key);
        if (it != g_deviceDispatch.end()) {
            return it->second(device, pName);
        }
        return nullptr;
    }

    __declspec(dllexport) VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL VRInject_vkGetInstanceProcAddr(VkInstance instance, const char* pName) {
        if (strcmp(pName, "vkCreateInstance") == 0) return reinterpret_cast<PFN_vkVoidFunction>(VRInject_vkCreateInstance);
        if (strcmp(pName, "vkCreateDevice") == 0) return reinterpret_cast<PFN_vkVoidFunction>(VRInject_vkCreateDevice);
        
        // Instance-level functions
        if (strcmp(pName, "vkQueuePresentKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(VRInject_QueuePresentKHR);
        if (strcmp(pName, "vkCreateSwapchainKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(VRInject_vkCreateSwapchainKHR);
        if (strcmp(pName, "vkGetSwapchainImagesKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(VRInject_vkGetSwapchainImagesKHR);
        if (strcmp(pName, "vkMapMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(VRInject_vkMapMemory);
        if (strcmp(pName, "vkCmdUpdateBuffer") == 0) return reinterpret_cast<PFN_vkVoidFunction>(VRInject_vkCmdUpdateBuffer);

        void* key = GetDispatchKey(instance);
        auto it = g_instanceDispatch.find(key);
        if (it != g_instanceDispatch.end()) {
            return it->second(instance, pName);
        }
        return nullptr;
    }

    __declspec(dllexport) VKAPI_ATTR VkResult VKAPI_CALL VRInject_vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface* pVersionStruct) {
        if (pVersionStruct->loaderLayerInterfaceVersion >= 2) {
            pVersionStruct->pfnGetInstanceProcAddr = VRInject_vkGetInstanceProcAddr;
            pVersionStruct->pfnGetDeviceProcAddr = VRInject_vkGetDeviceProcAddr;
            pVersionStruct->pfnGetPhysicalDeviceProcAddr = nullptr;
        }
        return VK_SUCCESS;
    }
}
