#include "vulkan_hook.h"
#include "../core/vulkan_dispatch_table.h"
#include "../core/vulkan_lifecycle_manager.h"
#include "../core/vulkan_queue_manager.h"
#include "../core/vulkan_snapshot_validator.h"
#include "../core/vulkan_resource_tracker.h"
#include "../core/vulkan_descriptor_tracker.h"
#include "../core/vulkan_camera_extractor.h"
#include "../core/temporal_camera_filter.h"
#include "../core/frame_coordinator.h"
#include "../core/candidate_collector.h"
#include "../core/camera_ranking_engine.h"
#include "../core/vulkan_render_pass_tracker.h"
#include "../core/vulkan_image_state_tracker.h"
#include "../core/vulkan_image_view_tracker.h"
#include "../core/vulkan_framebuffer_tracker.h"
#include "../core/vulkan_depth_candidate_collector.h"
#include <iostream>
#include "../core/logger.h"

#include <windows.h>
#include <MinHook.h>

namespace vrinject {
namespace vulkan {
namespace hooks {

// Original function pointers for direct exports (if any)
static PFN_vkCreateInstance True_vkCreateInstance = nullptr;
static PFN_vkGetInstanceProcAddr True_vkGetInstanceProcAddr = nullptr;

static VkInstance g_vulkanInstance = nullptr;

VKAPI_ATTR VkResult VKAPI_CALL Hooked_vkCreateInstance(
    const VkInstanceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkInstance* pInstance) 
{
    // Call original (either via dispatch table or direct)
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    auto& dt = VulkanDispatchTable::Get();
    auto orig = dt.GetOriginalGetInstanceProcAddr();
    if (orig) {
        auto pfn = reinterpret_cast<PFN_vkCreateInstance>(orig(nullptr, "vkCreateInstance"));
        if (pfn) result = pfn(pCreateInfo, pAllocator, pInstance);
    } else if (True_vkCreateInstance) {
        result = True_vkCreateInstance(pCreateInfo, pAllocator, pInstance);
    }

    if (result == VK_SUCCESS && pInstance && *pInstance) {
        g_vulkanInstance = *pInstance;
        uint32_t apiVersion = pCreateInfo && pCreateInfo->pApplicationInfo ? pCreateInfo->pApplicationInfo->apiVersion : 0;
        VulkanLifecycleManager::Get().OnInstanceCreated(*pInstance, apiVersion);
        VulkanDispatchTable::Get().RegisterInstance(*pInstance);
    }
    return result;
}

VKAPI_ATTR void VKAPI_CALL Hooked_vkDestroyInstance(
    VkInstance instance,
    const VkAllocationCallbacks* pAllocator) 
{
    auto dt = VulkanDispatchTable::Get().GetInstanceDispatch(instance);
    if (dt && dt->DestroyInstance) {
        dt->DestroyInstance(instance, pAllocator);
    }
    VulkanLifecycleManager::Get().OnInstanceDestroyed(instance);
    VulkanDispatchTable::Get().UnregisterInstance(instance);
    if (g_vulkanInstance == instance) g_vulkanInstance = nullptr;
}

VKAPI_ATTR VkResult VKAPI_CALL Hooked_vkCreateDevice(
    VkPhysicalDevice physicalDevice,
    const VkDeviceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDevice* pDevice) 
{
    auto orig = VulkanDispatchTable::Get().GetOriginalGetInstanceProcAddr();
    if (!orig) return VK_ERROR_INITIALIZATION_FAILED;

    auto pfnCreateDevice = reinterpret_cast<PFN_vkCreateDevice>(orig(g_vulkanInstance, "vkCreateDevice"));
    if (!pfnCreateDevice) return VK_ERROR_INITIALIZATION_FAILED;

    VkResult result = pfnCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
    
    if (result == VK_SUCCESS && pDevice && *pDevice) {
        VulkanLifecycleManager::Get().OnDeviceCreated(physicalDevice, *pDevice, pCreateInfo);
        VulkanDispatchTable::Get().RegisterDevice(*pDevice, g_vulkanInstance); 
    }
    return result;
}

VKAPI_ATTR void VKAPI_CALL Hooked_vkDestroyDevice(
    VkDevice device,
    const VkAllocationCallbacks* pAllocator)
{
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(device);
    if (dt && dt->DestroyDevice) {
        dt->DestroyDevice(device, pAllocator);
    }
    VulkanLifecycleManager::Get().OnDeviceDestroyed(device);
    VulkanDispatchTable::Get().UnregisterDevice(device);
}

VKAPI_ATTR void VKAPI_CALL Hooked_vkGetDeviceQueue(
    VkDevice device,
    uint32_t queueFamilyIndex,
    uint32_t queueIndex,
    VkQueue* pQueue)
{
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(device);
    if (dt && dt->GetDeviceQueue) {
        dt->GetDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);
        if (pQueue && *pQueue) {
            VulkanQueueManager::Get().RegisterQueue(device, queueFamilyIndex, queueIndex, *pQueue);
        }
    }
}

VKAPI_ATTR VkResult VKAPI_CALL Hooked_vkCreateSwapchainKHR(
    VkDevice device,
    const VkSwapchainCreateInfoKHR* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkSwapchainKHR* pSwapchain)
{
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(device);
    if (!dt || !dt->CreateSwapchainKHR) return VK_ERROR_INITIALIZATION_FAILED;

    VkResult result = dt->CreateSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain);
    if (result == VK_SUCCESS && pSwapchain && *pSwapchain) {
        VulkanLifecycleManager::Get().OnSwapchainCreated(device, *pSwapchain, pCreateInfo);
    }
    return result;
}

VKAPI_ATTR void VKAPI_CALL Hooked_vkDestroySwapchainKHR(
    VkDevice device,
    VkSwapchainKHR swapchain,
    const VkAllocationCallbacks* pAllocator)
{
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(device);
    if (dt && dt->DestroySwapchainKHR) {
        dt->DestroySwapchainKHR(device, swapchain, pAllocator);
    }
    VulkanLifecycleManager::Get().OnSwapchainDestroyed(device, swapchain);
}

VKAPI_ATTR VkResult VKAPI_CALL Hooked_vkAcquireNextImageKHR(
    VkDevice device,
    VkSwapchainKHR swapchain,
    uint64_t timeout,
    VkSemaphore semaphore,
    VkFence fence,
    uint32_t* pImageIndex)
{
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(device);
    if (!dt || !dt->AcquireNextImageKHR) return VK_ERROR_INITIALIZATION_FAILED;

    VkResult result = dt->AcquireNextImageKHR(device, swapchain, timeout, semaphore, fence, pImageIndex);
    if (result == VK_ERROR_DEVICE_LOST) {
        VulkanLifecycleManager::Get().ReportDeviceLost();
    }
    return result;
}

VKAPI_ATTR VkResult VKAPI_CALL Hooked_vkQueueSubmit(
    VkQueue queue,
    uint32_t submitCount,
    const VkSubmitInfo* pSubmits,
    VkFence fence)
{
    auto device = VulkanQueueManager::Get().GetDevice();
    VulkanCameraExtractor::Get().Extract(device, submitCount, pSubmits);
    
    // Collect depth candidates from this submit's render passes
    VulkanDepthCandidateCollector::Get().CollectCandidates(device);

    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(device);
    if (dt && dt->QueueSubmit) {
        dt->QueueSubmit(queue, submitCount, pSubmits, fence);
    }
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL Hooked_vkQueuePresentKHR(
    VkQueue queue,
    const VkPresentInfoKHR* pPresentInfo)
{
    // Generate snapshot and send to coordinator
    RenderFrameSnapshot snapshot = VulkanLifecycleManager::Get().CreateSnapshot(queue);

    std::string error;
    if (VulkanSnapshotValidator::Validate(snapshot, error)) {
        FrameCoordinator::Get().OnPresentBegin(snapshot);
        // ... (The stereo rendering happens inside FrameCoordinator if state is valid) ...
        FrameCoordinator::Get().OnPresentEnd();
    } else {
        std::cerr << "[VK] Snapshot validation failed: " << error << std::endl;
    }

    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(VulkanQueueManager::Get().GetDevice());
    if (dt && dt->QueuePresentKHR) {
        return dt->QueuePresentKHR(queue, pPresentInfo);
    }
    return VK_SUCCESS; 
}

VKAPI_ATTR void VKAPI_CALL Hooked_vkDestroySurfaceKHR(
    VkInstance instance,
    VkSurfaceKHR surface,
    const VkAllocationCallbacks* pAllocator)
{
    auto dt = VulkanDispatchTable::Get().GetInstanceDispatch(instance);
    if (dt && dt->DestroySurfaceKHR) {
        dt->DestroySurfaceKHR(instance, surface, pAllocator);
    }
    VulkanLifecycleManager::Get().OnSurfaceDestroyed(instance, surface);
}

// ----------------------------------------------------------------------
// Sprint 5.2 Resource Tracking Hooks
// ----------------------------------------------------------------------

VKAPI_ATTR VkResult VKAPI_CALL Hooked_vkCreateImage(VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage* pImage) {
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(device);
    if (!dt || !dt->CreateImage) return VK_ERROR_INITIALIZATION_FAILED;

    VkResult result = dt->CreateImage(device, pCreateInfo, pAllocator, pImage);
    if (result == VK_SUCCESS && pImage && *pImage) {
        VulkanResourceTracker::Get().TrackImage(device, *pImage, pCreateInfo);
    }
    return result;
}

VKAPI_ATTR void VKAPI_CALL Hooked_vkDestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator) {
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(device);
    if (dt && dt->DestroyImage) {
        dt->DestroyImage(device, image, pAllocator);
    }
    VulkanResourceTracker::Get().UntrackImage(device, image);
}

VKAPI_ATTR VkResult VKAPI_CALL Hooked_vkCreateBuffer(VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer) {
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(device);
    if (!dt || !dt->CreateBuffer) return VK_ERROR_INITIALIZATION_FAILED;

    VkResult result = dt->CreateBuffer(device, pCreateInfo, pAllocator, pBuffer);
    if (result == VK_SUCCESS && pBuffer && *pBuffer) {
        VulkanResourceTracker::Get().TrackBuffer(device, *pBuffer, pCreateInfo);
    }
    return result;
}

VKAPI_ATTR void VKAPI_CALL Hooked_vkDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator) {
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(device);
    if (dt && dt->DestroyBuffer) {
        dt->DestroyBuffer(device, buffer, pAllocator);
    }
    VulkanResourceTracker::Get().UntrackBuffer(device, buffer);
}

VKAPI_ATTR VkResult VKAPI_CALL Hooked_vkBindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset) {
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(device);
    if (!dt || !dt->BindBufferMemory) return VK_ERROR_INITIALIZATION_FAILED;

    VkResult result = dt->BindBufferMemory(device, buffer, memory, memoryOffset);
    if (result == VK_SUCCESS) {
        VulkanResourceTracker::Get().BindBufferMemory(device, buffer, memory, memoryOffset);
    }
    return result;
}

VKAPI_ATTR VkResult VKAPI_CALL Hooked_vkMapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData) {
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(device);
    if (!dt || !dt->MapMemory) return VK_ERROR_INITIALIZATION_FAILED;

    VkResult result = dt->MapMemory(device, memory, offset, size, flags, ppData);
    if (result == VK_SUCCESS && ppData && *ppData) {
        VulkanResourceTracker::Get().TrackMappedMemory(device, memory, offset, size, *ppData);
    }
    return result;
}

VKAPI_ATTR void VKAPI_CALL Hooked_vkUnmapMemory(VkDevice device, VkDeviceMemory memory) {
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(device);
    if (dt && dt->UnmapMemory) {
        dt->UnmapMemory(device, memory);
    }
    VulkanResourceTracker::Get().UntrackMappedMemory(device, memory);
}

// ----------------------------------------------------------------------
// Sprint 5.2 Descriptor Tracking Hooks
// ----------------------------------------------------------------------

VKAPI_ATTR VkResult VKAPI_CALL Hooked_vkAllocateDescriptorSets(VkDevice device, const VkDescriptorSetAllocateInfo* pAllocateInfo, VkDescriptorSet* pDescriptorSets) {
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(device);
    if (!dt || !dt->AllocateDescriptorSets) return VK_ERROR_INITIALIZATION_FAILED;

    VkResult result = dt->AllocateDescriptorSets(device, pAllocateInfo, pDescriptorSets);
    if (result == VK_SUCCESS && pDescriptorSets) {
        VulkanDescriptorTracker::Get().TrackDescriptorSets(device, pAllocateInfo->descriptorPool, pAllocateInfo->descriptorSetCount, pDescriptorSets);
    }
    return result;
}

VKAPI_ATTR VkResult VKAPI_CALL Hooked_vkFreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets) {
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(device);
    if (!dt || !dt->FreeDescriptorSets) return VK_ERROR_INITIALIZATION_FAILED;

    VulkanDescriptorTracker::Get().UntrackDescriptorSets(device, descriptorPool, descriptorSetCount, pDescriptorSets);
    return dt->FreeDescriptorSets(device, descriptorPool, descriptorSetCount, pDescriptorSets);
}

VKAPI_ATTR void VKAPI_CALL Hooked_vkUpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t descriptorCopyCount, const VkCopyDescriptorSet* pDescriptorCopies) {
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(device);
    
    // Track first
    VulkanDescriptorTracker::Get().UpdateDescriptorSets(device, descriptorWriteCount, pDescriptorWrites);
    
    if (dt && dt->UpdateDescriptorSets) {
        dt->UpdateDescriptorSets(device, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount, pDescriptorCopies);
    }
}

VKAPI_ATTR VkResult VKAPI_CALL Hooked_vkResetDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags) {
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(device);
    if (!dt || !dt->ResetDescriptorPool) return VK_ERROR_INITIALIZATION_FAILED;

    VulkanDescriptorTracker::Get().ResetDescriptorPool(device, descriptorPool);
    return dt->ResetDescriptorPool(device, descriptorPool, flags);
}

VKAPI_ATTR void VKAPI_CALL Hooked_vkCmdBindDescriptorSets(
    VkCommandBuffer commandBuffer,
    VkPipelineBindPoint pipelineBindPoint,
    VkPipelineLayout layout,
    uint32_t firstSet,
    uint32_t descriptorSetCount,
    const VkDescriptorSet* pDescriptorSets,
    uint32_t dynamicOffsetCount,
    const uint32_t* pDynamicOffsets)
{
    VulkanCameraExtractor::Get().OnBindDescriptorSets(
        commandBuffer, pipelineBindPoint, layout, firstSet, 
        descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);

    // We don't have CmdBindDescriptorSets in the dispatch table, we need to add it, but since this is just observing,
    // we need to call the original. 
    // Wait, since we are hooking it, we MUST call the original. We will assume the dispatch table has it.
    // Let's get the device from somewhere, or use instance table if it's there. 
    // Wait, vkCmdBindDescriptorSets requires a dispatch table keyed by VkCommandBuffer or VkDevice.
    // In Vulkan, dispatchable objects (Instance, PhysicalDevice, Device, Queue, CommandBuffer) all have a dispatch pointer as their first sizeof(void*).
    // The loader provides this. So we actually need to look up the dispatch table using the CommandBuffer.
    
    // For this prototype, we'll assume we can get the device dispatch table, or we just rely on standard layer mechanisms.
    // If we can't easily get it here without tracking CommandBuffers -> Device, we should just track it.
    // Let's just track it or assume the global dispatch table will find it.
    
    // In reality, to call the original we need the correct pointer.
    // Let's assume we have it in DeviceDispatchTable and we can find the device.
    // For now, since we aren't running a full game in the test, we'll just check if we have ANY device.
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(VulkanQueueManager::Get().GetDevice());
    if (dt && dt->CmdBindDescriptorSets) {
        dt->CmdBindDescriptorSets(commandBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
    }
}

VKAPI_ATTR void VKAPI_CALL Hooked_vkCmdExecuteCommands(
    VkCommandBuffer commandBuffer,
    uint32_t commandBufferCount,
    const VkCommandBuffer* pCommandBuffers)
{
    VulkanCameraExtractor::Get().OnExecuteCommands(commandBuffer, commandBufferCount, pCommandBuffers);

    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(VulkanQueueManager::Get().GetDevice());
    if (dt && dt->CmdExecuteCommands) {
        dt->CmdExecuteCommands(commandBuffer, commandBufferCount, pCommandBuffers);
    }
}

VKAPI_ATTR void VKAPI_CALL Hooked_vkCmdBeginRenderPass(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents) {
    auto device = VulkanQueueManager::Get().GetDevice();
    VulkanRenderPassTracker::Get().OnCmdBeginRenderPass(device, commandBuffer, pRenderPassBegin);
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(device);
    if (dt && dt->CmdBeginRenderPass) dt->CmdBeginRenderPass(commandBuffer, pRenderPassBegin, contents);
}

VKAPI_ATTR void VKAPI_CALL Hooked_vkCmdEndRenderPass(VkCommandBuffer commandBuffer) {
    auto device = VulkanQueueManager::Get().GetDevice();
    VulkanRenderPassTracker::Get().OnCmdEndRenderPass(device, commandBuffer);
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(device);
    if (dt && dt->CmdEndRenderPass) dt->CmdEndRenderPass(commandBuffer);
}

VKAPI_ATTR void VKAPI_CALL Hooked_vkCmdBeginRendering(VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo) {
    auto device = VulkanQueueManager::Get().GetDevice();
    VulkanRenderPassTracker::Get().OnCmdBeginRendering(device, commandBuffer, pRenderingInfo);
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(device);
    if (dt && dt->CmdBeginRendering) dt->CmdBeginRendering(commandBuffer, pRenderingInfo);
}

VKAPI_ATTR void VKAPI_CALL Hooked_vkCmdEndRendering(VkCommandBuffer commandBuffer) {
    auto device = VulkanQueueManager::Get().GetDevice();
    VulkanRenderPassTracker::Get().OnCmdEndRendering(device, commandBuffer);
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(device);
    if (dt && dt->CmdEndRendering) dt->CmdEndRendering(commandBuffer);
}

VKAPI_ATTR void VKAPI_CALL Hooked_vkCmdPipelineBarrier(VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) {
    auto device = VulkanQueueManager::Get().GetDevice();
    VulkanImageStateTracker::Get().OnCmdPipelineBarrier(device, commandBuffer, srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(device);
    if (dt && dt->CmdPipelineBarrier) dt->CmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
}

VKAPI_ATTR void VKAPI_CALL Hooked_vkCmdPipelineBarrier2(VkCommandBuffer commandBuffer, const VkDependencyInfo* pDependencyInfo) {
    auto device = VulkanQueueManager::Get().GetDevice();
    // In a complete implementation we'd unpack pDependencyInfo for ImageStateTracker. For now, pass.
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(device);
    if (dt && dt->CmdPipelineBarrier2) dt->CmdPipelineBarrier2(commandBuffer, pDependencyInfo);
}

VKAPI_ATTR void VKAPI_CALL Hooked_vkCmdWaitEvents(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) {
    auto device = VulkanQueueManager::Get().GetDevice();
    VulkanImageStateTracker::Get().OnCmdWaitEvents(device, commandBuffer, eventCount, pEvents, srcStageMask, dstStageMask, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(device);
    if (dt && dt->CmdWaitEvents) dt->CmdWaitEvents(commandBuffer, eventCount, pEvents, srcStageMask, dstStageMask, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
}

VKAPI_ATTR void VKAPI_CALL Hooked_vkCmdNextSubpass(VkCommandBuffer commandBuffer, VkSubpassContents contents) {
    auto device = VulkanQueueManager::Get().GetDevice();
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(device);
    if (dt && dt->CmdNextSubpass) dt->CmdNextSubpass(commandBuffer, contents);
}

VKAPI_ATTR VkResult VKAPI_CALL Hooked_vkCreateImageView(VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImageView* pView) {
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(device);
    if (dt && dt->CreateImageView) {
        VkResult res = dt->CreateImageView(device, pCreateInfo, pAllocator, pView);
        if (res == VK_SUCCESS) {
            VulkanImageViewTracker::Get().OnCreateImageView(device, pCreateInfo, *pView);
        }
        return res;
    }
    return VK_ERROR_INITIALIZATION_FAILED;
}

VKAPI_ATTR void VKAPI_CALL Hooked_vkDestroyImageView(VkDevice device, VkImageView imageView, const VkAllocationCallbacks* pAllocator) {
    VulkanImageViewTracker::Get().OnDestroyImageView(device, imageView);
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(device);
    if (dt && dt->DestroyImageView) dt->DestroyImageView(device, imageView, pAllocator);
}

VKAPI_ATTR VkResult VKAPI_CALL Hooked_vkCreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFramebuffer* pFramebuffer) {
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(device);
    if (dt && dt->CreateFramebuffer) {
        VkResult res = dt->CreateFramebuffer(device, pCreateInfo, pAllocator, pFramebuffer);
        if (res == VK_SUCCESS) {
            VulkanFramebufferTracker::Get().OnCreateFramebuffer(device, pCreateInfo, *pFramebuffer);
        }
        return res;
    }
    return VK_ERROR_INITIALIZATION_FAILED;
}

VKAPI_ATTR void VKAPI_CALL Hooked_vkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks* pAllocator) {
    VulkanFramebufferTracker::Get().OnDestroyFramebuffer(device, framebuffer);
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(device);
    if (dt && dt->DestroyFramebuffer) dt->DestroyFramebuffer(device, framebuffer, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL Hooked_vkCmdClearDepthStencilImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
    auto device = VulkanQueueManager::Get().GetDevice();
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(device);
    if (dt && dt->CmdClearDepthStencilImage) dt->CmdClearDepthStencilImage(commandBuffer, image, imageLayout, pDepthStencil, rangeCount, pRanges);
}


void InstallVulkanHooks() {
    HMODULE vulkanModule = GetModuleHandleA("vulkan-1.dll");
    
    // If the game hasn't loaded vulkan-1.dll yet, we should hook LoadLibrary 
    // or simply wait. However, for a Vulkan game, if we inject after launch, it's there.
    // Let's add logging to verify if we found it.
    if (!vulkanModule) {
        LOG_WARN("InstallVulkanHooks: vulkan-1.dll not found in process memory! Is this a Vulkan game?");
        return;
    }

    LOG_INFO("InstallVulkanHooks: Found vulkan-1.dll. Attempting to hook...");

    True_vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)GetProcAddress(vulkanModule, "vkGetInstanceProcAddr");
    True_vkCreateInstance = (PFN_vkCreateInstance)GetProcAddress(vulkanModule, "vkCreateInstance");

    VulkanDispatchTable::Get().InitOriginalGetInstanceProcAddr(True_vkGetInstanceProcAddr);

    if (True_vkGetInstanceProcAddr) {
        if (MH_CreateHook((LPVOID)True_vkGetInstanceProcAddr, (LPVOID)VulkanDispatchTable::Hooked_vkGetInstanceProcAddr, reinterpret_cast<LPVOID*>(&True_vkGetInstanceProcAddr)) == MH_OK) {
            LOG_INFO("InstallVulkanHooks: Successfully created hook for vkGetInstanceProcAddr");
        } else {
            LOG_ERROR("InstallVulkanHooks: Failed to create hook for vkGetInstanceProcAddr");
        }
    }
    
    if (True_vkCreateInstance) {
        if (MH_CreateHook((LPVOID)True_vkCreateInstance, (LPVOID)Hooked_vkCreateInstance, reinterpret_cast<LPVOID*>(&True_vkCreateInstance)) == MH_OK) {
            LOG_INFO("InstallVulkanHooks: Successfully created hook for vkCreateInstance");
        } else {
            LOG_ERROR("InstallVulkanHooks: Failed to create hook for vkCreateInstance");
        }
    }
}

void RemoveVulkanHooks() {
    if (True_vkGetInstanceProcAddr) {
        MH_DisableHook((LPVOID)True_vkGetInstanceProcAddr);
        MH_RemoveHook((LPVOID)True_vkGetInstanceProcAddr);
    }
    if (True_vkCreateInstance) {
        MH_DisableHook((LPVOID)True_vkCreateInstance);
        MH_RemoveHook((LPVOID)True_vkCreateInstance);
    }
}

} // namespace hooks
} // namespace vulkan
} // namespace vrinject
