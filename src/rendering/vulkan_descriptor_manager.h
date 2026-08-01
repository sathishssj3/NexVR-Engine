#pragma once

#include <vulkan/vulkan.h>
#include <mutex>
#include <vector>

namespace vrinject {
namespace vulkan {

class VulkanDescriptorManager {
public:
    VulkanDescriptorManager(VkDevice device, VkDescriptorSetLayout layout);
    ~VulkanDescriptorManager();

    bool AllocateSets(uint32_t count);
    void DestroyPool();

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
};

} // namespace vulkan
} // namespace vrinject
