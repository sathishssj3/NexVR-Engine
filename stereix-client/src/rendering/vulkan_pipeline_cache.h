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
    bool SavePersistentCache();
    void SetPersistentCachePath(const std::string& path) { m_cachePath = path; }

    VkPipeline GetStereoPipeline() const { return m_stereoPipeline; }
    VkPipelineLayout GetPipelineLayout() const { return m_pipelineLayout; }
    VkDescriptorSetLayout GetDescriptorSetLayout() const { return m_descriptorSetLayout; }
    uint64_t GetPersistentCacheBytesLoaded() const { return m_cacheBytesLoaded; }
    uint64_t GetPersistentCacheBytesSaved() const { return m_cacheBytesSaved; }

private:
    std::vector<char> LoadPersistentCacheData();
    bool CreateDescriptorSetLayout();
    bool CreatePipelineLayout();
    bool LoadShaderModule(const std::string& filename, VkShaderModule* outModule);
    bool CreateComputePipeline();

    VkDevice m_device = VK_NULL_HANDLE;
    std::mutex m_mutex;
    bool m_safeCreationPhase = true;
    std::string m_cachePath = "nexvr_vulkan_pipeline_cache.bin";
    uint64_t m_cacheBytesLoaded = 0;
    uint64_t m_cacheBytesSaved = 0;

    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkShaderModule m_stereoShaderModule = VK_NULL_HANDLE;
    VkPipeline m_stereoPipeline = VK_NULL_HANDLE;
    VkPipelineCache m_pipelineCache = VK_NULL_HANDLE;
};

} // namespace vulkan
} // namespace vrinject
