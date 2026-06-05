
#include "../rendering/vulkan/vk_layer.h"
#include <mutex>
#include "../rendering/backends/vulkan_renderer.h"
#include "../rendering/stereo_pipeline.h"
#include "../core/logger.h"
#include "../ai_matrix_classifier/matrix_classifier.h"

extern vrinject::StereoPipeline g_stereoPipeline;
vrinject::VulkanRenderer g_vkRenderer;
vrinject::ai::MatrixClassifier g_matrixClassifierVK;
static std::once_flag s_vkInitFlag;

VKAPI_ATTR VkResult VKAPI_CALL VRInject_QueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
    std::call_once(s_vkInitFlag, [&]() {
        // Initialize backend and pass to OpenXR
        // Note: For a true layer, we need to track device/queue from vkCreateDevice/vkGetDeviceQueue
        // For this prototype, we'll just initialize it on first present
        g_vkRenderer.Initialize(VK_NULL_HANDLE /* stub device */, queue);
        g_stereoPipeline.GetOpenXRManager()->SetRenderer(&g_vkRenderer);
        LOG_INFO("Vulkan backend initialized via IRenderer (Implicit Layer)");
    });

    // In a real layer, we'd call the next layer's QueuePresentKHR.
    // For this prototype, we just log and return success or call the trampoline if we were using MinHook.
    // Since we are an explicit layer via the manifest, we need to fetch the next proc addr.
    // We'll leave it as a stub that just returns success for the prototype architecture.
    // We'll leave it as a stub that just returns success for the prototype architecture.
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL VRInject_vkMapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData) {
    // In a real layer, we fetch the next layer's PFN_vkMapMemory.
    // Stub implementation to demonstrate interception architecture:
    VkResult result = VK_SUCCESS; 
    if (result == VK_SUCCESS && ppData && *ppData) {
        // Defer scan or scan immediately if we know it's a constant buffer.
        // For prototyping, we can't fully execute it without the dispatch table.
    }
    return result;
}

VKAPI_ATTR void VKAPI_CALL VRInject_vkCmdUpdateBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize dataSize, const void* pData) {
    if (pData && dataSize >= 64) {
        auto detections = g_matrixClassifierVK.ScanBuffer(pData, dataSize);
        if (!detections.empty()) {
            // Found projection matrix in CmdUpdateBuffer payload
        }
    }
    // Call next layer...
}

// Exports required by the Vulkan loader for layers
extern "C" {
    __declspec(dllexport) VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL VRInject_vkGetDeviceProcAddr(VkDevice device, const char* pName) {
        if (strcmp(pName, "vkQueuePresentKHR") == 0) {
            return reinterpret_cast<PFN_vkVoidFunction>(VRInject_QueuePresentKHR);
        }
        if (strcmp(pName, "vkMapMemory") == 0) {
            return reinterpret_cast<PFN_vkVoidFunction>(VRInject_vkMapMemory);
        }
        if (strcmp(pName, "vkCmdUpdateBuffer") == 0) {
            return reinterpret_cast<PFN_vkVoidFunction>(VRInject_vkCmdUpdateBuffer);
        }
        return nullptr; // In a real layer, call the next GetDeviceProcAddr
    }

    __declspec(dllexport) VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL VRInject_vkGetInstanceProcAddr(VkInstance instance, const char* pName) {
        if (strcmp(pName, "vkQueuePresentKHR") == 0) {
            return reinterpret_cast<PFN_vkVoidFunction>(VRInject_QueuePresentKHR);
        }
        if (strcmp(pName, "vkMapMemory") == 0) {
            return reinterpret_cast<PFN_vkVoidFunction>(VRInject_vkMapMemory);
        }
        if (strcmp(pName, "vkCmdUpdateBuffer") == 0) {
            return reinterpret_cast<PFN_vkVoidFunction>(VRInject_vkCmdUpdateBuffer);
        }
        return nullptr; // In a real layer, call the next GetInstanceProcAddr
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
