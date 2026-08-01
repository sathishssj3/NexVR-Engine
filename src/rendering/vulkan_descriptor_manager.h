#pragma once

#include <vulkan/vulkan.h>
#include <mutex>
#include <vector>

namespace vrinject {
namespace vulkan {

struct VulkanDescriptorRecycleStats {
    uint32_t allocatedSets = 0;
    uint32_t recycledSets = 0;
    uint32_t poolGrowthCount = 0;
    float fragmentationRatio = 0.0f;
};

class VulkanDescriptorManager {
public:
    VulkanDescriptorManager(VkDevice device, VkDescriptorSetLayout layout);
    ~VulkanDescriptorManager();

    bool AllocateSets(uint32_t count);
    bool EnsureCapacity(uint32_t count);
    void RecycleDescriptorSet(uint32_t frameIndex);
    void DestroyPool();
    VulkanDescriptorRecycleStats GetRecycleStats() const { return m_stats; }

    VkDescriptorSet GetDescriptorSet(uint32_t frameIndex) const;

    void UpdateDescriptorSets(uint32_t frameIndex,
                              VkBuffer cameraBuffer, 
                              VkDeviceSize cameraBufferOffset,
                              VkDeviceSize cameraBufferSize,
                              VkImageView depthView, 
                              VkSampler depthSampler,
                              VkImageView leftEyeView, 
                              VkImageView rightEyeView);

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_layout = VK_NULL_HANDLE;
    std::mutex m_mutex;

    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_descriptorSets;
    std::vector<bool> m_recycled;
    VulkanDescriptorRecycleStats m_stats{};
};

} // namespace vulkan
} // namespace vrinject
