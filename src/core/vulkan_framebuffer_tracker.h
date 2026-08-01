#pragma once

#include <vulkan/vulkan.h>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace vrinject {
namespace vulkan {

struct VulkanFramebufferRecord {
    VkFramebuffer framebuffer = nullptr;
    VkDevice device = nullptr;
    VkRenderPass renderPass = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t layers = 0;
    std::vector<VkImageView> attachments;
};

class VulkanFramebufferTracker {
public:
    static VulkanFramebufferTracker& Get() {
        static VulkanFramebufferTracker instance;
        return instance;
    }

    void OnCreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, VkFramebuffer framebuffer);
    void OnDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer);

    bool GetFramebufferRecord(VkDevice device, VkFramebuffer framebuffer, VulkanFramebufferRecord& outRecord) const;

private:
    VulkanFramebufferTracker() = default;

    mutable std::shared_mutex m_mutex;
    
    struct DeviceFbKey {
        VkDevice device;
        VkFramebuffer fb;
        bool operator==(const DeviceFbKey& other) const {
            return device == other.device && fb == other.fb;
        }
    };
    struct DeviceFbKeyHash {
        std::size_t operator()(const DeviceFbKey& k) const {
            return std::hash<VkDevice>()(k.device) ^ (std::hash<VkFramebuffer>()(k.fb) << 1);
        }
    };

    std::unordered_map<DeviceFbKey, VulkanFramebufferRecord, DeviceFbKeyHash> m_framebuffers;
};

} // namespace vulkan
} // namespace vrinject
