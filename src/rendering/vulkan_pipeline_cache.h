#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <mutex>

namespace vrinject {
namespace vulkan {

class VulkanPipelineCache {
public:
    VulkanPipelineCache(VkDevice device);
    ~VulkanPipelineCache();

    bool Initialize();
    void EndInitializationPhase();

    VkPipeline GetStereoPipeline() const { return m_stereoPipeline; }
    VkPipelineLayout GetPipelineLayout() const { return m_pipelineLayout; }
    VkDescriptorSetLayout GetDescriptorSetLayout() const { return m_descriptorSetLayout; }

private:
    bool CreateDescriptorSetLayout();
    bool CreatePipelineLayout();
    bool LoadShaderModule(const std::string& filename, VkShaderModule* outModule);
    bool CreateComputePipeline();

    VkDevice m_device = VK_NULL_HANDLE;
    std::mutex m_mutex;
    bool m_safeCreationPhase = true;

    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkShaderModule m_stereoShaderModule = VK_NULL_HANDLE;
    VkPipeline m_stereoPipeline = VK_NULL_HANDLE;
    VkPipelineCache m_pipelineCache = VK_NULL_HANDLE;
};

} // namespace vulkan
} // namespace vrinject
