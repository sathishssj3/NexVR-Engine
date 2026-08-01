#include "vulkan_descriptor_manager.h"
#include "../core/vulkan_dispatch_table.h"
#include <stdexcept>

namespace vrinject {
namespace vulkan {

VulkanDescriptorManager::VulkanDescriptorManager(VkDevice device, VkDescriptorSetLayout layout)
    : m_device(device), m_layout(layout) {
}

VulkanDescriptorManager::~VulkanDescriptorManager() {
    DestroyPool();
}

void VulkanDescriptorManager::DestroyPool() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_descriptorPool) {
        auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(m_device);
        if (dt) dt->DestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
    m_descriptorSets.clear();
}

bool VulkanDescriptorManager::AllocateSets(uint32_t count) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(m_device);
    if (!dt) return false;

    // Destroy old pool if exists
    if (m_descriptorPool) {
        dt->DestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
        m_descriptorSets.clear();
    }

    VkDescriptorPoolSize poolSizes[3]{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = count; // Camera
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = count; // Depth
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[2].descriptorCount = count * 2; // Left/Right UAVs

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 3;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = count;

    if (dt->CreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) return false;

    std::vector<VkDescriptorSetLayout> layouts(count, m_layout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = count;
    allocInfo.pSetLayouts = layouts.data();

    m_descriptorSets.resize(count);
    return dt->AllocateDescriptorSets(m_device, &allocInfo, m_descriptorSets.data()) == VK_SUCCESS;
}

VkDescriptorSet VulkanDescriptorManager::GetDescriptorSet(uint32_t frameIndex) const {
    if (frameIndex < m_descriptorSets.size()) {
        return m_descriptorSets[frameIndex];
    }
    return VK_NULL_HANDLE;
}

void VulkanDescriptorManager::UpdateDescriptorSets(uint32_t frameIndex,
                                                   VkBuffer cameraBuffer, 
                                                   VkDeviceSize cameraBufferOffset,
                                                   VkDeviceSize cameraBufferSize,
                                                   VkImageView depthView, 
                                                   VkSampler depthSampler,
                                                   VkImageView leftEyeView, 
                                                   VkImageView rightEyeView) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (frameIndex >= m_descriptorSets.size()) return;

    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(m_device);
    if (!dt) return;

    VkDescriptorSet set = m_descriptorSets[frameIndex];
    VkWriteDescriptorSet writes[4]{};

    // Binding 0: Camera Buffer
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = cameraBuffer;
    bufferInfo.offset = cameraBufferOffset;
    bufferInfo.range = cameraBufferSize;

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = set;
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &bufferInfo;

    // Binding 1: Depth Texture
    VkDescriptorImageInfo depthInfo{};
    depthInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // Assume it's transitioned to this
    depthInfo.imageView = depthView;
    depthInfo.sampler = depthSampler;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = set;
    writes[1].dstBinding = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &depthInfo;

    // Binding 2: Left Eye
    VkDescriptorImageInfo leftInfo{};
    leftInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    leftInfo.imageView = leftEyeView;

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = set;
    writes[2].dstBinding = 2;
    writes[2].dstArrayElement = 0;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[2].descriptorCount = 1;
    writes[2].pImageInfo = &leftInfo;

    // Binding 3: Right Eye
    VkDescriptorImageInfo rightInfo{};
    rightInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    rightInfo.imageView = rightEyeView;

    writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet = set;
    writes[3].dstBinding = 3;
    writes[3].dstArrayElement = 0;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[3].descriptorCount = 1;
    writes[3].pImageInfo = &rightInfo;

    dt->UpdateDescriptorSets(m_device, 4, writes, 0, nullptr);
}

} // namespace vulkan
} // namespace vrinject
