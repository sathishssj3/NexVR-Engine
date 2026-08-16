#include "rendering/vulkan_pipeline_cache.h"
#include "rendering/vulkan/vulkan_dispatch_table.h"
#include <fstream>
#include <cassert>

namespace vrinject {
namespace vulkan {

VulkanPipelineCache::VulkanPipelineCache(VkDevice device) : m_device(device) {
}

VulkanPipelineCache::~VulkanPipelineCache() {
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(m_device);
    if (dt) {
        SavePersistentCache();
        if (m_stereoPipeline) dt->DestroyPipeline(m_device, m_stereoPipeline, nullptr);
        if (m_pipelineLayout) dt->DestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        if (m_descriptorSetLayout) dt->DestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
        if (m_stereoShaderModule) dt->DestroyShaderModule(m_device, m_stereoShaderModule, nullptr);
        if (m_pipelineCache) dt->DestroyPipelineCache(m_device, m_pipelineCache, nullptr);
    }
}

bool VulkanPipelineCache::Initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    assert(m_safeCreationPhase && "Pipeline creation attempted outside initialization phase");

    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(m_device);
    if (!dt) return false;

    std::vector<char> cacheData = LoadPersistentCacheData();

    VkPipelineCacheCreateInfo cacheInfo{};
    cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    cacheInfo.initialDataSize = cacheData.size();
    cacheInfo.pInitialData = cacheData.empty() ? nullptr : cacheData.data();
    dt->CreatePipelineCache(m_device, &cacheInfo, nullptr, &m_pipelineCache);

    if (!CreateDescriptorSetLayout()) return false;
    if (!CreatePipelineLayout()) return false;
    if (!LoadShaderModule("shaders/vulkan/stereo.spv", &m_stereoShaderModule)) {
        // Fallback or warning
    }
    if (!CreateComputePipeline()) return false;

    return true;
}

void VulkanPipelineCache::EndInitializationPhase() {
    m_safeCreationPhase = false;
}

std::vector<char> VulkanPipelineCache::LoadPersistentCacheData() {
    m_cacheBytesLoaded = 0;
    std::ifstream file(m_cachePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {};

    const std::streamsize size = file.tellg();
    if (size <= 0) return {};

    std::vector<char> data(static_cast<size_t>(size));
    file.seekg(0);
    file.read(data.data(), size);
    if (!file) return {};

    m_cacheBytesLoaded = static_cast<uint64_t>(data.size());
    return data;
}

bool VulkanPipelineCache::SavePersistentCache() {
    if (!m_pipelineCache) return false;

    size_t size = 0;
    if (vkGetPipelineCacheData(m_device, m_pipelineCache, &size, nullptr) != VK_SUCCESS || size == 0) {
        return false;
    }

    std::vector<char> data(size);
    if (vkGetPipelineCacheData(m_device, m_pipelineCache, &size, data.data()) != VK_SUCCESS) {
        return false;
    }

    std::ofstream file(m_cachePath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) return false;

    file.write(data.data(), static_cast<std::streamsize>(size));
    if (!file) return false;

    m_cacheBytesSaved = static_cast<uint64_t>(size);
    return true;
}

bool VulkanPipelineCache::CreateDescriptorSetLayout() {
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(m_device);

    VkDescriptorSetLayoutBinding bindings[4]{};
    // Camera Buffer
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Depth Texture
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Left Eye UAV
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Right Eye UAV
    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 4;
    layoutInfo.pBindings = bindings;

    return dt->CreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayout) == VK_SUCCESS;
}

bool VulkanPipelineCache::CreatePipelineLayout() {
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(m_device);

    VkPushConstantRange pushConstant{};
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstant.offset = 0;
    pushConstant.size = 128; // Large enough for basic constants

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstant;

    return dt->CreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) == VK_SUCCESS;
}

bool VulkanPipelineCache::LoadShaderModule(const std::string& filename, VkShaderModule* outModule) {
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(m_device);

    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) return false;

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = buffer.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

    return dt->CreateShaderModule(m_device, &createInfo, nullptr, outModule) == VK_SUCCESS;
}

bool VulkanPipelineCache::CreateComputePipeline() {
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(m_device);

    if (!m_stereoShaderModule) return false;

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = m_pipelineLayout;
    
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = m_stereoShaderModule;
    pipelineInfo.stage.pName = "main";

    return dt->CreateComputePipelines(m_device, m_pipelineCache, 1, &pipelineInfo, nullptr, &m_stereoPipeline) == VK_SUCCESS;
}

} // namespace vulkan
} // namespace vrinject
