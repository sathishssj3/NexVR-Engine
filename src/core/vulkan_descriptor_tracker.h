#pragma once

#include <vulkan/vulkan.h>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <shared_mutex>
#include "vulkan_resource_tracker.h" // For DeviceHandleHash

namespace vrinject {
namespace vulkan {

struct DescriptorBinding {
    uint32_t binding = 0;
    VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
    
    // Exactly one of these will be non-null depending on type
    VkBuffer buffer = nullptr;
    VkImageView imageView = nullptr;
    VkSampler sampler = nullptr;
    
    // For buffers
    VkDeviceSize offset = 0;
    VkDeviceSize range = 0;
    
    // For images
    VkImageLayout imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
};

struct VulkanDescriptorSetInfo {
    VkDescriptorSet set = nullptr;
    VkDescriptorPool pool = nullptr;
    VkDevice device = nullptr;
    
    // binding slot -> DescriptorBinding
    std::unordered_map<uint32_t, DescriptorBinding> bindings;
};

class VulkanDescriptorTracker {
public:
    static VulkanDescriptorTracker& Get() {
        static VulkanDescriptorTracker instance;
        return instance;
    }

    // Allocate / Free
    void TrackDescriptorSets(VkDevice device, VkDescriptorPool pool, uint32_t count, const VkDescriptorSet* pSets);
    void UntrackDescriptorSets(VkDevice device, VkDescriptorPool pool, uint32_t count, const VkDescriptorSet* pSets);
    
    // Pool Reset (Invalidates all sets in pool)
    void ResetDescriptorPool(VkDevice device, VkDescriptorPool pool);

    // Updates
    void UpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites);

    // Retrieval
    bool GetDescriptorSetInfo(VkDevice device, VkDescriptorSet set, VulkanDescriptorSetInfo& outInfo) const;

private:
    VulkanDescriptorTracker() = default;

    mutable std::shared_mutex m_mutex;

    // device + pool -> list of sets
    std::unordered_map<std::pair<VkDevice, VkDescriptorPool>, std::vector<VkDescriptorSet>, DeviceHandleHash> m_poolToSets;
    
    // device + set -> detailed info
    std::unordered_map<std::pair<VkDevice, VkDescriptorSet>, VulkanDescriptorSetInfo, DeviceHandleHash> m_sets;
};

} // namespace vulkan
} // namespace vrinject
