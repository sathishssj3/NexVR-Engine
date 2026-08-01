#include "vulkan_image_state_tracker.h"

namespace vrinject {
namespace vulkan {

void VulkanImageStateTracker::ProcessImageBarriers(
    VkDevice device,
    VkPipelineStageFlags dstStageMask,
    uint32_t imageMemoryBarrierCount,
    const VkImageMemoryBarrier* pImageMemoryBarriers)
{
    if (!device || imageMemoryBarrierCount == 0 || !pImageMemoryBarriers) return;

    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    for (uint32_t i = 0; i < imageMemoryBarrierCount; ++i) {
        const auto& barrier = pImageMemoryBarriers[i];
        if (!barrier.image) continue;

        DeviceImageKey key{device, barrier.image};
        auto& state = m_imageStates[key];
        
        state.image = barrier.image;
        state.device = device;
        state.previousLayout = barrier.oldLayout;
        state.currentLayout = barrier.newLayout;
        state.lastPipelineStage = dstStageMask;
        state.lastAccessMask = barrier.dstAccessMask;
        
        if (barrier.dstQueueFamilyIndex != VK_QUEUE_FAMILY_IGNORED) {
            state.queueFamilyOwner = barrier.dstQueueFamilyIndex;
        }
    }
}

void VulkanImageStateTracker::OnCmdPipelineBarrier(
    VkDevice device,
    VkCommandBuffer commandBuffer,
    VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags dstStageMask,
    VkDependencyFlags dependencyFlags,
    uint32_t memoryBarrierCount,
    const VkMemoryBarrier* pMemoryBarriers,
    uint32_t bufferMemoryBarrierCount,
    const VkBufferMemoryBarrier* pBufferMemoryBarriers,
    uint32_t imageMemoryBarrierCount,
    const VkImageMemoryBarrier* pImageMemoryBarriers)
{
    ProcessImageBarriers(device, dstStageMask, imageMemoryBarrierCount, pImageMemoryBarriers);
}

void VulkanImageStateTracker::OnCmdWaitEvents(
    VkDevice device,
    VkCommandBuffer commandBuffer,
    uint32_t eventCount,
    const VkEvent* pEvents,
    VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags dstStageMask,
    uint32_t memoryBarrierCount,
    const VkMemoryBarrier* pMemoryBarriers,
    uint32_t bufferMemoryBarrierCount,
    const VkBufferMemoryBarrier* pBufferMemoryBarriers,
    uint32_t imageMemoryBarrierCount,
    const VkImageMemoryBarrier* pImageMemoryBarriers)
{
    ProcessImageBarriers(device, dstStageMask, imageMemoryBarrierCount, pImageMemoryBarriers);
}

bool VulkanImageStateTracker::GetImageState(VkDevice device, VkImage image, ImageStateRecord& outState) const {
    if (!device || !image) return false;

    std::shared_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_imageStates.find({device, image});
    if (it != m_imageStates.end()) {
        outState = it->second;
        return true;
    }
    return false;
}

} // namespace vulkan
} // namespace vrinject
