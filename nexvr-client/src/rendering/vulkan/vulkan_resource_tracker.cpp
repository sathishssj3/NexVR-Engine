#include "rendering/vulkan/vulkan_resource_tracker.h"

namespace vrinject {
namespace vulkan {

void VulkanResourceTracker::TrackImage(VkDevice device, VkImage image, const VkImageCreateInfo* pCreateInfo) {
    if (!device || !image || !pCreateInfo) return;

    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    VulkanImageInfo info;
    info.image = image;
    info.device = device;
    info.format = pCreateInfo->format;
    info.extent = pCreateInfo->extent;
    info.usage = pCreateInfo->usage;
    info.mipLevels = pCreateInfo->mipLevels;
    info.arrayLayers = pCreateInfo->arrayLayers;
    info.generation = m_currentGeneration++;

    m_images[{device, image}] = info;
}

void VulkanResourceTracker::UntrackImage(VkDevice device, VkImage image) {
    if (!device || !image) return;

    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_images.erase({device, image});
}

bool VulkanResourceTracker::GetImageInfo(VkDevice device, VkImage image, VulkanImageInfo& outInfo) const {
    if (!device || !image) return false;

    std::shared_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_images.find({device, image});
    if (it != m_images.end()) {
        outInfo = it->second;
        return true;
    }
    return false;
}

void VulkanResourceTracker::TrackBuffer(VkDevice device, VkBuffer buffer, const VkBufferCreateInfo* pCreateInfo) {
    if (!device || !buffer || !pCreateInfo) return;

    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    VulkanBufferInfo info;
    info.buffer = buffer;
    info.device = device;
    info.size = pCreateInfo->size;
    info.usage = pCreateInfo->usage;
    info.generation = m_currentGeneration++;

    m_buffers[{device, buffer}] = info;
}

void VulkanResourceTracker::BindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset) {
    if (!device || !buffer || !memory) return;

    std::unique_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_buffers.find({device, buffer});
    if (it != m_buffers.end()) {
        it->second.boundMemory = memory;
        it->second.memoryOffset = memoryOffset;
    }
}

void VulkanResourceTracker::UntrackBuffer(VkDevice device, VkBuffer buffer) {
    if (!device || !buffer) return;

    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_buffers.erase({device, buffer});
}

bool VulkanResourceTracker::GetBufferInfo(VkDevice device, VkBuffer buffer, VulkanBufferInfo& outInfo) const {
    if (!device || !buffer) return false;

    std::shared_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_buffers.find({device, buffer});
    if (it != m_buffers.end()) {
        outInfo = it->second;
        return true;
    }
    return false;
}

void VulkanResourceTracker::TrackMappedMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, void* hostPointer) {
    if (!device || !memory || !hostPointer) return;

    std::unique_lock<std::shared_mutex> lock(m_mutex);
    VulkanMappedMemory info;
    info.memory = memory;
    info.device = device;
    info.hostPointer = hostPointer;
    info.offset = offset;
    info.size = size;

    m_mappedMemory[{device, memory}] = info;
    
    // In the future, this is where we would notify the UniversalScanner
    // SubsystemContext::Get().GetUniversalScanner()->RegisterMemoryRegion(hostPointer, size);
}

void VulkanResourceTracker::UntrackMappedMemory(VkDevice device, VkDeviceMemory memory) {
    if (!device || !memory) return;

    std::unique_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_mappedMemory.find({device, memory});
    if (it != m_mappedMemory.end()) {
        // SubsystemContext::Get().GetUniversalScanner()->UnregisterMemoryRegion(it->second.hostPointer);
        m_mappedMemory.erase(it);
    }
}

std::vector<VulkanMappedMemory> VulkanResourceTracker::GetAllMappedMemory() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    std::vector<VulkanMappedMemory> result;
    result.reserve(m_mappedMemory.size());
    for (const auto& pair : m_mappedMemory) {
        result.push_back(pair.second);
    }
    return result;
}

} // namespace vulkan
} // namespace vrinject
