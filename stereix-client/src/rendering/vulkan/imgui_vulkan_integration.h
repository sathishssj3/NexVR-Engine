#pragma once
#include <vulkan/vulkan.h>
#include <mutex>

namespace vrinject {
namespace vulkan {

struct VulkanCachedFramebuffer {
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
};

class ImGuiVulkanIntegration {
public:
    static ImGuiVulkanIntegration& GetInstance() {
        static ImGuiVulkanIntegration instance;
        return instance;
    }

    bool Initialize(
        VkInstance instance,
        VkPhysicalDevice physicalDevice,
        VkDevice device,
        uint32_t queueFamily,
        VkQueue queue,
        VkFormat colorFormat
    );

    void Render(VkCommandBuffer cmdBuffer, VkFramebuffer framebuffer, VkExtent2D extent);
    void Render(VkCommandBuffer cmdBuffer, VkImage leftDest, VkImage rightDest, VkExtent2D extent);
    void Shutdown();

    bool IsInitialized() const { return m_initialized; }

private:
    ImGuiVulkanIntegration() = default;
    ~ImGuiVulkanIntegration() = default;

    VkFramebuffer GetOrCreateFramebuffer(VkImage image, VulkanCachedFramebuffer& cached, VkExtent2D extent);
    void DestroyCachedFramebuffer(VulkanCachedFramebuffer& cached);

    bool m_initialized = false;
    VkDevice m_device = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkFormat m_colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    VulkanCachedFramebuffer m_cachedLeft;
    VulkanCachedFramebuffer m_cachedRight;
    std::mutex m_mutex;
};

} // namespace vulkan
} // namespace vrinject
