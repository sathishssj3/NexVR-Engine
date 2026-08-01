#pragma once

#include <vulkan/vulkan.h>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace vrinject {
namespace vulkan {

struct VulkanImageViewRecord {
    VkImageView imageView = nullptr;
    VkDevice device = nullptr;
    VkImage image = nullptr;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageSubresourceRange subresourceRange{};
};

class VulkanImageViewTracker {
public:
    static VulkanImageViewTracker& Get() {
        static VulkanImageViewTracker instance;
        return instance;
    }

    void OnCreateImageView(VkDevice device, const VkImageViewCreateInfo* pCreateInfo, VkImageView imageView);
    void OnDestroyImageView(VkDevice device, VkImageView imageView);

    bool GetImageViewRecord(VkDevice device, VkImageView imageView, VulkanImageViewRecord& outRecord) const;

private:
    VulkanImageViewTracker() = default;

    mutable std::shared_mutex m_mutex;
    
    struct DeviceViewKey {
        VkDevice device;
        VkImageView view;
        bool operator==(const DeviceViewKey& other) const {
            return device == other.device && view == other.view;
        }
    };
    struct DeviceViewKeyHash {
        std::size_t operator()(const DeviceViewKey& k) const {
            return std::hash<VkDevice>()(k.device) ^ (std::hash<VkImageView>()(k.view) << 1);
        }
    };

    std::unordered_map<DeviceViewKey, VulkanImageViewRecord, DeviceViewKeyHash> m_imageViews;
};

} // namespace vulkan
} // namespace vrinject
