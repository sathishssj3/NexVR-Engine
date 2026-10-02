#pragma once
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <shared_mutex>

namespace vrinject {
namespace vulkan {

struct BoundDescriptorSet {
    VkDescriptorSet set = nullptr;
    std::vector<uint32_t> dynamicOffsets;
};

struct CommandBufferState {
    VkPipelineLayout pipelineLayout = nullptr;
    // Map from set index to bound set
    std::unordered_map<uint32_t, BoundDescriptorSet> boundSets;
    // Secondary command buffers executed by this primary
    std::vector<VkCommandBuffer> executedSecondaries;
};

class VulkanCameraExtractor {
public:
    static VulkanCameraExtractor& Get() {
        static VulkanCameraExtractor instance;
        return instance;
    }

    // Called on vkCmdBindDescriptorSets
    void OnBindDescriptorSets(
        VkCommandBuffer commandBuffer,
        VkPipelineBindPoint pipelineBindPoint,
        VkPipelineLayout layout,
        uint32_t firstSet,
        uint32_t descriptorSetCount,
        const VkDescriptorSet* pDescriptorSets,
        uint32_t dynamicOffsetCount,
        const uint32_t* pDynamicOffsets);

    // Called on vkCmdExecuteCommands
    void OnExecuteCommands(
        VkCommandBuffer commandBuffer,
        uint32_t commandBufferCount,
        const VkCommandBuffer* pCommandBuffers);

    // Called on vkQueueSubmit
    void Extract(VkDevice device, uint32_t submitCount, const VkSubmitInfo* pSubmits);

private:
    VulkanCameraExtractor() = default;

    std::shared_mutex m_mutex;
    std::unordered_map<VkCommandBuffer, CommandBufferState> m_commandBufferStates;
};

} // namespace vulkan
} // namespace vrinject
