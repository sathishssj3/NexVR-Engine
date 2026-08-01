#pragma once

#include <vulkan/vulkan.h>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <atomic>

namespace vrinject {
namespace vulkan {

struct ResourceState {
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkAccessFlags accessMask = 0;
    uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
};

class VulkanResourceStateTracker {
public:
    VulkanResourceStateTracker(VkDevice device);
    ~VulkanResourceStateTracker() = default;

    void SetState(VkImage image, const ResourceState& state);
    ResourceState GetState(VkImage image) const;

    // Generates a barrier only if the requested state differs from current
    bool TransitionImage(VkCommandBuffer cmd, 
                         VkImage image, 
                         VkImageLayout newLayout,
                         VkPipelineStageFlags dstStageMask,
                         VkAccessFlags dstAccessMask,
                         uint32_t dstQueueFamilyIndex, 
                         VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT);

    // Forces a resource state without emitting a barrier. Used to track externally acquired images (e.g. OpenXR swapchain)
    void ForceResourceState(VkImage image, VkImageLayout layout, VkPipelineStageFlags stageMask, VkAccessFlags accessMask, uint32_t queueFamilyIndex);

    void TransitionImages(VkCommandBuffer cmd,
                          const std::vector<VkImage>& images,
                          VkImageLayout newLayout,
                          VkPipelineStageFlags dstStageMask,
                          VkAccessFlags dstAccessMask,
                          uint32_t dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                          VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT);

    uint64_t GetBarrierCount() const { return m_barrierCount.load(); }
    void ResetBarrierCount() { m_barrierCount.store(0); }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    mutable std::mutex m_mutex;
    std::unordered_map<VkImage, ResourceState> m_states;
    std::atomic<uint64_t> m_barrierCount{0};
};

} // namespace vulkan
} // namespace vrinject
