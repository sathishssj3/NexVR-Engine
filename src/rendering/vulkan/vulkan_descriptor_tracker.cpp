#include "rendering/vulkan/vulkan_descriptor_tracker.h"
#include <algorithm>

namespace vrinject {
namespace vulkan {

void VulkanDescriptorTracker::TrackDescriptorSets(VkDevice device, VkDescriptorPool pool, uint32_t count, const VkDescriptorSet* pSets) {
    if (!device || !pool || !pSets || count == 0) return;

    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    auto& poolSets = m_poolToSets[{device, pool}];
    
    for (uint32_t i = 0; i < count; ++i) {
        if (pSets[i]) {
            poolSets.push_back(pSets[i]);
            
            VulkanDescriptorSetInfo info;
            info.set = pSets[i];
            info.pool = pool;
            info.device = device;
            m_sets[{device, pSets[i]}] = std::move(info);
        }
    }
}

void VulkanDescriptorTracker::UntrackDescriptorSets(VkDevice device, VkDescriptorPool pool, uint32_t count, const VkDescriptorSet* pSets) {
    if (!device || !pool || !pSets || count == 0) return;

    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    auto poolIt = m_poolToSets.find({device, pool});
    if (poolIt != m_poolToSets.end()) {
        auto& poolSets = poolIt->second;
        for (uint32_t i = 0; i < count; ++i) {
            if (pSets[i]) {
                // Remove from pool list
                poolSets.erase(std::remove(poolSets.begin(), poolSets.end(), pSets[i]), poolSets.end());
                // Remove from sets map
                m_sets.erase({device, pSets[i]});
            }
        }
    }
}

void VulkanDescriptorTracker::ResetDescriptorPool(VkDevice device, VkDescriptorPool pool) {
    if (!device || !pool) return;

    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    auto poolIt = m_poolToSets.find({device, pool});
    if (poolIt != m_poolToSets.end()) {
        for (VkDescriptorSet set : poolIt->second) {
            m_sets.erase({device, set});
        }
        poolIt->second.clear();
    }
}

void VulkanDescriptorTracker::UpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites) {
    if (!device || !pDescriptorWrites || descriptorWriteCount == 0) return;

    std::unique_lock<std::shared_mutex> lock(m_mutex);

    for (uint32_t i = 0; i < descriptorWriteCount; ++i) {
        const auto& write = pDescriptorWrites[i];
        
        auto setIt = m_sets.find({device, write.dstSet});
        if (setIt != m_sets.end()) {
            // Vulkan allows updating multiple consecutive array elements
            for (uint32_t arrayEl = 0; arrayEl < write.descriptorCount; ++arrayEl) {
                uint32_t targetBindingIndex = write.dstBinding + arrayEl;
                
                DescriptorBinding binding;
                binding.binding = targetBindingIndex;
                binding.descriptorType = write.descriptorType;
                
                if (write.pImageInfo && (
                    write.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
                    write.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
                    write.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
                    write.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
                    write.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)) 
                {
                    binding.imageView = write.pImageInfo[arrayEl].imageView; // Note: Typically imageView, but keeping handle struct simple for now. 
                    // Technically we should map ImageView -> Image, but for observation this is okay.
                    binding.sampler = write.pImageInfo[arrayEl].sampler;
                    binding.imageLayout = write.pImageInfo[arrayEl].imageLayout;
                }
                else if (write.pBufferInfo && (
                    write.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
                    write.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
                    write.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
                    write.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)) 
                {
                    binding.buffer = write.pBufferInfo[arrayEl].buffer;
                    binding.offset = write.pBufferInfo[arrayEl].offset;
                    binding.range = write.pBufferInfo[arrayEl].range;
                }
                
                setIt->second.bindings[targetBindingIndex] = binding;
            }
        }
    }
}

bool VulkanDescriptorTracker::GetDescriptorSetInfo(VkDevice device, VkDescriptorSet set, VulkanDescriptorSetInfo& outInfo) const {
    if (!device || !set) return false;

    std::shared_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_sets.find({device, set});
    if (it != m_sets.end()) {
        outInfo = it->second;
        return true;
    }
    return false;
}

} // namespace vulkan
} // namespace vrinject
