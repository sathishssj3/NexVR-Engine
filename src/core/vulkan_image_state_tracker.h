#pragma once

#include <vulkan/vulkan.h>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace vrinject {
namespace vulkan {

struct ImageStateRecord {
    VkImage image = nullptr;
    VkDevice device = nullptr;
    VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout previousLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkPipelineStageFlags lastPipelineStage = 0;
    VkAccessFlags lastAccessMask = 0;
    uint32_t queueFamilyOwner = VK_QUEUE_FAMILY_IGNORED;
};

class VulkanImageStateTracker {
public:
    static VulkanImageStateTracker& Get() {
        static VulkanImageStateTracker instance;
        return instance;
    }

    void OnCmdPipelineBarrier(
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
        const VkImageMemoryBarrier* pImageMemoryBarriers);

    void OnCmdWaitEvents(
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
        const VkImageMemoryBarrier* pImageMemoryBarriers);

    bool GetImageState(VkDevice device, VkImage image, ImageStateRecord& outState) const;

private:
    VulkanImageStateTracker() = default;

    void ProcessImageBarriers(
        VkDevice device,
        VkPipelineStageFlags dstStageMask,
        uint32_t imageMemoryBarrierCount,
        const VkImageMemoryBarrier* pImageMemoryBarriers);

    mutable std::shared_mutex m_mutex;
    
    struct DeviceImageKey {
        VkDevice device;
        VkImage image;
        bool operator==(const DeviceImageKey& other) const {
            return device == other.device && image == other.image;
        }
    };
    struct DeviceImageKeyHash {
        std::size_t operator()(const DeviceImageKey& k) const {
            return std::hash<VkDevice>()(k.device) ^ (std::hash<VkImage>()(k.image) << 1);
        }
    };

    std::unordered_map<DeviceImageKey, ImageStateRecord, DeviceImageKeyHash> m_imageStates;
};

} // namespace vulkan
} // namespace vrinject
