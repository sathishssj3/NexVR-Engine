#pragma once

#include "igraphics_backend.h"
#include "stereo_pipeline.h"
#include <vulkan/vulkan.h>
#include <memory>
#include <mutex>
#include "../ai/ai_scheduler.h"
#include "vulkan_memory_budget.h"

namespace vrinject {
namespace vulkan {

class VulkanStereoRenderer;
class VulkanStereoResourceManager;
class VulkanPipelineCache;
class VulkanCommandManager;
class VulkanSyncManager;
class VulkanDescriptorManager;
class VulkanResourceStateTracker;

class VulkanGraphicsBackend : public IGraphicsBackend {
public:
    VulkanGraphicsBackend();
    ~VulkanGraphicsBackend() override;

    // IGraphicsBackend implementation
    bool Initialize(void* nativeDevice, void* nativeContext) override;
    void Shutdown() override;
    void SetState(StereoRendererState state) override;
    StereoRendererState GetState() const override;
    bool Healthy() const override;
    CameraSnapshot GetCamera() override;
    DepthSnapshot GetDepth() override;
    void RenderStereo(const CameraSnapshot& camSnapshot, const DepthSnapshot& depthSnapshot, const StereoParams& params) override;
    void* GetLeftEyeTexture() override;
    void* GetRightEyeTexture() override;
    GraphicsBackend GetAPI() const override { return GraphicsBackend::Vulkan; }

    // OpenXR Interop
    void SetOpenXRSwapchainImages(VkImage left, VkImage right);

    void Resize(uint32_t width, uint32_t height);
    void WaitForGPU();
    VulkanResourceStateTracker* GetStateTracker() const;

    // Vulkan-specific initialization with physical device for proper resource allocation
    bool InitializeVulkan(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue, uint32_t queueFamilyIndex);

private:
    bool m_isInitialized = false;
    StereoRendererState m_state = StereoRendererState::UNINITIALIZED;
    CameraSnapshot m_lastCamera;
    DepthSnapshot m_lastDepth;
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    std::mutex m_mutex;

    // Subsystems
    std::unique_ptr<VulkanStereoResourceManager> m_resourceManager;
    std::unique_ptr<VulkanPipelineCache> m_pipelineCache;
    std::unique_ptr<VulkanDescriptorManager> m_descriptorManager;
    std::unique_ptr<VulkanCommandManager> m_commandManager;
    std::unique_ptr<VulkanSyncManager> m_syncManager;
    std::unique_ptr<VulkanResourceStateTracker> m_stateTracker;
    std::unique_ptr<VulkanStereoRenderer> m_renderer;
    std::unique_ptr<vrinject::ai::AIScheduler> m_aiQueue;
    std::unique_ptr<VulkanMemoryBudget> m_memoryBudget;

    // GPU resources owned by the backend
    VkBuffer m_cameraBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_cameraBufferMemory = VK_NULL_HANDLE;
    VkImage m_depthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_depthImageMemory = VK_NULL_HANDLE;
    VkImageView m_depthImageView = VK_NULL_HANDLE;

    bool CreateGPUResources();
    void DestroyGPUResources();
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    VkImage m_oxrLeftDest = VK_NULL_HANDLE;
    VkImage m_oxrRightDest = VK_NULL_HANDLE;
};

} // namespace vulkan
} // namespace vrinject
