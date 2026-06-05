#pragma once
#include "../irenderer.h"

// We define VK_NO_PROTOTYPES or similar depending on the project,
// but for simplicity we'll just include the standard vulkan header.
// Windows platform define required before vulkan.h
#define VK_USE_PLATFORM_WIN32_KHR
#include "../vulkan/vulkan.h"

namespace vrinject {

class VulkanRenderer : public IRenderer {
public:
    bool Initialize(void* nativeDevice, void* nativeContext) override;
    void Shutdown() override;

    TextureHandle CreateTexture(uint32_t width, uint32_t height,
                                uint32_t format, bool uav) override;
    void DestroyTexture(TextureHandle& handle) override;

    ShaderHandle LoadComputeShader(const uint8_t* bytecode,
                                   size_t bytecodeSize) override;
    void DestroyShader(ShaderHandle& handle) override;

    void DispatchCompute(ShaderHandle shader,
                         TextureHandle input, TextureHandle output,
                         uint32_t groupsX, uint32_t groupsY) override;

    void CopyToSwapchain(TextureHandle source,
                         void* swapchainTexture) override;

    GraphicsAPI GetAPI() const override { return GraphicsAPI::VULKAN; }

    VkDevice         GetDevice()        const { return m_device; }
    VkPhysicalDevice GetPhysicalDevice() const { return m_physicalDevice; }
    VkInstance       GetInstance()      const { return m_instance; }

    // Required for OpenXR
    void SetVulkanContext(VkInstance instance, VkPhysicalDevice physDevice) {
        m_instance = instance;
        m_physicalDevice = physDevice;
    }

private:
    VkInstance       m_instance       = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice         m_device         = VK_NULL_HANDLE;
    VkQueue          m_queue          = VK_NULL_HANDLE;
    uint32_t         m_queueFamilyIndex = 0;

    VkCommandPool    m_cmdPool        = VK_NULL_HANDLE;
    VkCommandBuffer  m_cmdBuffer      = VK_NULL_HANDLE;
    VkDescriptorPool m_descPool       = VK_NULL_HANDLE;
};

} // namespace vrinject
