#include "rendering/vulkan_resource_state_tracker.h"
#include "rendering/vulkan/vulkan_dispatch_table.h"

namespace vrinject {
namespace vulkan {

VulkanResourceStateTracker::VulkanResourceStateTracker(VkDevice device) : m_device(device) {
}

void VulkanResourceStateTracker::SetState(VkImage image, const ResourceState& state) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_states[image] = state;
}

ResourceState VulkanResourceStateTracker::GetState(VkImage image) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_states.find(image);
    if (it != m_states.end()) {
        return it->second;
    }
    return ResourceState{}; // Default is undefined layout, top of pipe
}

void VulkanResourceStateTracker::ForceResourceState(VkImage image, VkImageLayout layout, VkPipelineStageFlags stageMask, VkAccessFlags accessMask, uint32_t queueFamilyIndex) {
    std::lock_guard<std::mutex> lock(m_mutex);
    ResourceState state;
    state.layout = layout;
    state.stageMask = stageMask;
    state.accessMask = accessMask;
    state.queueFamilyIndex = queueFamilyIndex;
    m_states[image] = state;
}

bool VulkanResourceStateTracker::TransitionImage(VkCommandBuffer cmd,
                                                 VkImage image, 
                                                 VkImageLayout newLayout,
                                                 VkPipelineStageFlags dstStageMask,
                                                 VkAccessFlags dstAccessMask,
                                                 uint32_t dstQueueFamily,
                                                 VkImageAspectFlags aspectMask) {
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(m_device);
    if (!dt) return false;

    ResourceState currentState;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        currentState = m_states[image]; // defaults to undefined if not found
    }

    if (currentState.layout == newLayout && 
        currentState.queueFamilyIndex == dstQueueFamily && 
        (currentState.accessMask & dstAccessMask) == dstAccessMask) {
        return false; // No transition needed
    }

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = currentState.layout;
    barrier.newLayout = newLayout;

    // Only set queue family indices if doing a real queue family ownership transfer.
    // Otherwise use VK_QUEUE_FAMILY_IGNORED for both to avoid validation errors.
    bool isQueueFamilyTransfer = (currentState.queueFamilyIndex != VK_QUEUE_FAMILY_IGNORED &&
                                   dstQueueFamily != VK_QUEUE_FAMILY_IGNORED &&
                                   currentState.queueFamilyIndex != dstQueueFamily);
    barrier.srcQueueFamilyIndex = isQueueFamilyTransfer ? currentState.queueFamilyIndex : VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = isQueueFamilyTransfer ? dstQueueFamily : VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;

    barrier.subresourceRange.aspectMask = aspectMask;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    barrier.srcAccessMask = currentState.accessMask;
    barrier.dstAccessMask = dstAccessMask;

    dt->CmdPipelineBarrier(cmd,
                           currentState.stageMask == 0 ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : currentState.stageMask,
                           dstStageMask == 0 ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT : dstStageMask,
                           0, 0, nullptr, 0, nullptr, 1, &barrier);

    m_barrierCount++;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_states[image] = { newLayout, dstStageMask, dstAccessMask, dstQueueFamily };
    }

    return true;
}

void VulkanResourceStateTracker::TransitionImages(VkCommandBuffer cmd,
                                                  const std::vector<VkImage>& images,
                                                  VkImageLayout newLayout,
                                                  VkPipelineStageFlags dstStageMask,
                                                  VkAccessFlags dstAccessMask,
                                                  uint32_t dstQueueFamily,
                                                  VkImageAspectFlags aspectMask) {
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(m_device);
    if (!dt) return;

    std::vector<VkImageMemoryBarrier> barriers;
    barriers.reserve(images.size());
    VkPipelineStageFlags srcStageMask = 0;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (VkImage image : images) {
            ResourceState currentState = m_states[image];
            
            if (currentState.layout == newLayout && 
                currentState.queueFamilyIndex == dstQueueFamily && 
                (currentState.accessMask & dstAccessMask) == dstAccessMask) {
                continue;
            }

            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = currentState.layout;
            barrier.newLayout = newLayout;
            barrier.srcQueueFamilyIndex = currentState.queueFamilyIndex;
            barrier.dstQueueFamilyIndex = dstQueueFamily;
            barrier.image = image;
            barrier.subresourceRange.aspectMask = aspectMask;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.srcAccessMask = currentState.accessMask;
            barrier.dstAccessMask = dstAccessMask;

            barriers.push_back(barrier);
            srcStageMask |= (currentState.stageMask == 0 ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : currentState.stageMask);
            
            m_states[image] = { newLayout, dstStageMask, dstAccessMask, dstQueueFamily };
        }
    }

    if (!barriers.empty()) {
        dt->CmdPipelineBarrier(cmd,
                               srcStageMask,
                               dstStageMask == 0 ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT : dstStageMask,
                               0, 0, nullptr, 0, nullptr, (uint32_t)barriers.size(), barriers.data());
        m_barrierCount += barriers.size();
    }
}

} // namespace vulkan
} // namespace vrinject
