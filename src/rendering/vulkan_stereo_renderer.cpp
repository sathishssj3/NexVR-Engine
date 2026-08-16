#include "rendering/vulkan_stereo_renderer.h"
#include "rendering/vulkan/vulkan_dispatch_table.h"
#include <iostream>

namespace vrinject {
namespace vulkan {

VulkanStereoRenderer::VulkanStereoRenderer(VkDevice device, VkQueue queue, uint32_t queueFamilyIndex)
    : m_device(device), m_queue(queue), m_queueFamilyIndex(queueFamilyIndex) {
}

VulkanStereoRenderer::~VulkanStereoRenderer() {
}

bool VulkanStereoRenderer::Initialize() {
    return m_device != VK_NULL_HANDLE && m_queue != VK_NULL_HANDLE;
}

bool VulkanStereoRenderer::Render(const CameraSnapshot& camera,
                                  const DepthSnapshot& depth,
                                  VkBuffer cameraBuffer,
                                  VkDeviceSize cameraBufferSize,
                                  VkImageView depthView,
                                  VulkanStereoResourceManager& resourceManager,
                                  VulkanPipelineCache& pipelineCache,
                                  VulkanDescriptorManager& descriptorManager,
                                  VulkanCommandManager& commandManager,
                                  VulkanSyncManager& syncManager,
                                  VulkanResourceStateTracker& stateTracker,
                                  VkImage oxrLeftDest,
                                  VkImage oxrRightDest,
                                  VkImageView uiMaskView) {
    
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(m_device);
    if (!dt) {
        std::cerr << "Render early return: dt is null\n";
        return false;
    }

    // 1. Sync — wait for any previous GPU work on this queue
    if (!syncManager.WaitForFrame(m_frameIndex)) {
        std::cerr << "Render early return: WaitForFrame failed for frame " << m_frameIndex << "\n";
        return false;
    }
    if (!syncManager.ResetFrame(m_frameIndex)) {
        std::cerr << "Render early return: ResetFrame failed for frame " << m_frameIndex << "\n";
        return false;
    }
    VkFence frameFence = syncManager.GetFence(m_frameIndex);
    if (frameFence == VK_NULL_HANDLE) {
        std::cerr << "Render early return: frame fence is null for frame " << m_frameIndex << "\n";
        return false;
    }

    // 2. Command Buffer
    VkCommandBuffer cmd = commandManager.BeginFrame(m_frameIndex);
    if (!cmd) {
        std::cerr << "Render early return: cmd is null\n";
        return false;
    }

    // 3. Transition depth image to SHADER_READ_ONLY_OPTIMAL
    VkImage depthImage = static_cast<VkImage>(depth.identity.nativeHandle);
    if (depthImage) {
        stateTracker.TransitionImage(cmd, depthImage, 
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                     VK_ACCESS_SHADER_READ_BIT,
                                     m_queueFamilyIndex,
                                     VK_IMAGE_ASPECT_DEPTH_BIT);
    }

    // 4. Transition eye output images to GENERAL for storage writes
    VkImage leftEyeImage = resourceManager.GetLeftEyeImage();
    VkImage rightEyeImage = resourceManager.GetRightEyeImage();
    if (leftEyeImage) {
        stateTracker.TransitionImage(cmd, leftEyeImage,
                                     VK_IMAGE_LAYOUT_GENERAL,
                                     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                     VK_ACCESS_SHADER_WRITE_BIT,
                                     m_queueFamilyIndex);
    }
    if (rightEyeImage) {
        stateTracker.TransitionImage(cmd, rightEyeImage,
                                     VK_IMAGE_LAYOUT_GENERAL,
                                     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                     VK_ACCESS_SHADER_WRITE_BIT,
                                     m_queueFamilyIndex);
    }

    // 5. Bind compute pipeline
    VkPipeline pipeline = pipelineCache.GetStereoPipeline();
    if (pipeline) {
        dt->CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    } else {
        std::cerr << "Render debug: pipeline is null\n";
    }

    // 6. Update & bind descriptor sets with real handles
    descriptorManager.UpdateDescriptorSets(m_frameIndex, 
                                           cameraBuffer, 0, cameraBufferSize,
                                           depthView,
                                           resourceManager.GetSampler(), 
                                           resourceManager.GetLeftEyeView(), 
                                           resourceManager.GetRightEyeView());

    VkDescriptorSet set = descriptorManager.GetDescriptorSet(m_frameIndex);
    if (set) {
        dt->CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, 
                                  pipelineCache.GetPipelineLayout(), 0, 1, &set, 0, nullptr);
    }

    // 7. Dispatch compute shader
    uint32_t groupCountX = (resourceManager.GetWidth() + 15) / 16;
    uint32_t groupCountY = (resourceManager.GetHeight() + 15) / 16;
    if (groupCountX > 0 && groupCountY > 0 && pipeline) {
        dt->CmdDispatch(cmd, groupCountX, groupCountY, 1);
    }

    // 8. Copy to OpenXR Swapchain if provided
    if (!oxrLeftDest) {
        std::cerr << "Render debug: oxrLeftDest is null\n";
    }
    if (!leftEyeImage) {
        std::cerr << "Render debug: leftEyeImage is null\n";
    }
    if (oxrLeftDest && leftEyeImage) {
        // Transition internal left eye from GENERAL to TRANSFER_SRC
        stateTracker.TransitionImage(cmd, leftEyeImage,
                                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     VK_ACCESS_TRANSFER_READ_BIT,
                                     m_queueFamilyIndex);
                                     
        // Transition OpenXR swapchain image to TRANSFER_DST
        // Assume it starts in UNDEFINED or whatever state ForceResourceState set it to
        stateTracker.TransitionImage(cmd, oxrLeftDest,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     VK_ACCESS_TRANSFER_WRITE_BIT,
                                     m_queueFamilyIndex);

        VkImageCopy copyRegion{};
        copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.srcSubresource.layerCount = 1;
        copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.dstSubresource.layerCount = 1;
        copyRegion.extent.width = resourceManager.GetWidth();
        copyRegion.extent.height = resourceManager.GetHeight();
        copyRegion.extent.depth = 1;

        dt->CmdCopyImage(cmd, leftEyeImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         oxrLeftDest, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         1, &copyRegion);
                         
        // Transition swapchain image to COLOR_ATTACHMENT_OPTIMAL (common for OpenXR release)
        stateTracker.TransitionImage(cmd, oxrLeftDest,
                                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                     VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                     m_queueFamilyIndex);
    }

    if (oxrRightDest && rightEyeImage) {
        // Transition internal right eye from GENERAL to TRANSFER_SRC
        stateTracker.TransitionImage(cmd, rightEyeImage,
                                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     VK_ACCESS_TRANSFER_READ_BIT,
                                     m_queueFamilyIndex);
                                     
        // Transition OpenXR swapchain image to TRANSFER_DST
        stateTracker.TransitionImage(cmd, oxrRightDest,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     VK_ACCESS_TRANSFER_WRITE_BIT,
                                     m_queueFamilyIndex);

        VkImageCopy copyRegion{};
        copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.srcSubresource.layerCount = 1;
        copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.dstSubresource.layerCount = 1;
        copyRegion.extent.width = resourceManager.GetWidth();
        copyRegion.extent.height = resourceManager.GetHeight();
        copyRegion.extent.depth = 1;

        dt->CmdCopyImage(cmd, rightEyeImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         oxrRightDest, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         1, &copyRegion);
                         
        // Transition swapchain image to COLOR_ATTACHMENT_OPTIMAL
        stateTracker.TransitionImage(cmd, oxrRightDest,
                                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                     VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                     m_queueFamilyIndex);
    }

    // 9. End and submit
    commandManager.EndFrame(m_frameIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    VkResult submitRes = dt->QueueSubmit(m_queue, 1, &submitInfo, frameFence);
    if (submitRes != VK_SUCCESS) {
        std::cerr << "Render early return: QueueSubmit failed with " << submitRes << "\n";
        return false;
    }

    m_frameIndex = (m_frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
    return true;
}

} // namespace vulkan
} // namespace vrinject
