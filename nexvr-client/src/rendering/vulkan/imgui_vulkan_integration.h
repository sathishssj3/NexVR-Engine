#pragma once
#include <vulkan/vulkan.h>
#include <mutex>

namespace vrinject {
namespace vulkan {

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
    void Shutdown();

    bool IsInitialized() const { return m_initialized; }

private:
    ImGuiVulkanIntegration() = default;
    ~ImGuiVulkanIntegration() = default;

    bool m_initialized = false;
    VkDevice m_device = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    std::mutex m_mutex;
};

} // namespace vulkan
} // namespace vrinject
