#pragma once

#include <vulkan/vulkan.h>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>
#include <memory>
#include "core/graphics_types.h" // For resource generation counters if we use them, or just use our own here.

namespace vrinject {
namespace vulkan {

struct VulkanImageInfo {
    VkImage image = nullptr;
    VkDevice device = nullptr;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent3D extent = {0, 0, 0};
    VkImageUsageFlags usage = 0;
    uint32_t mipLevels = 1;
    uint32_t arrayLayers = 1;
    uint64_t generation = 0;
};

struct VulkanBufferInfo {
    VkBuffer buffer = nullptr;
    VkDevice device = nullptr;
    VkDeviceSize size = 0;
    VkBufferUsageFlags usage = 0;
    uint64_t generation = 0;
    VkDeviceMemory boundMemory = nullptr;
    VkDeviceSize memoryOffset = 0;
};

struct VulkanMappedMemory {
    VkDeviceMemory memory = nullptr;
    VkDevice device = nullptr;
    void* hostPointer = nullptr;
    VkDeviceSize offset = 0;
    VkDeviceSize size = 0;
};

// Device-scoped hashing
struct DeviceHandleHash {
    template <typename T>
    std::size_t operator()(const std::pair<VkDevice, T>& pair) const {
        return std::hash<VkDevice>()(pair.first) ^ (std::hash<T>()(pair.second) << 1);
    }
};

class VulkanResourceTracker {
public:
    static VulkanResourceTracker& Get() {
        static VulkanResourceTracker instance;
        return instance;
    }

    // Image Tracking
    void TrackImage(VkDevice device, VkImage image, const VkImageCreateInfo* pCreateInfo);
    void UntrackImage(VkDevice device, VkImage image);
    bool GetImageInfo(VkDevice device, VkImage image, VulkanImageInfo& outInfo) const;

    // Buffer Tracking
    void TrackBuffer(VkDevice device, VkBuffer buffer, const VkBufferCreateInfo* pCreateInfo);
    void BindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset);
    void UntrackBuffer(VkDevice device, VkBuffer buffer);
    bool GetBufferInfo(VkDevice device, VkBuffer buffer, VulkanBufferInfo& outInfo) const;

    // Memory Mapping Tracking
    void TrackMappedMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, void* hostPointer);
    void UntrackMappedMemory(VkDevice device, VkDeviceMemory memory);
    std::vector<VulkanMappedMemory> GetAllMappedMemory() const;

private:
    VulkanResourceTracker() = default;

    mutable std::shared_mutex m_mutex;

    // Maps keyed by std::pair<VkDevice, Handle> to prevent cross-device collision
    std::unordered_map<std::pair<VkDevice, VkImage>, VulkanImageInfo, DeviceHandleHash> m_images;
    std::unordered_map<std::pair<VkDevice, VkBuffer>, VulkanBufferInfo, DeviceHandleHash> m_buffers;
    std::unordered_map<std::pair<VkDevice, VkDeviceMemory>, VulkanMappedMemory, DeviceHandleHash> m_mappedMemory;

    uint64_t m_currentGeneration = 1;
};

} // namespace vulkan
} // namespace vrinject
