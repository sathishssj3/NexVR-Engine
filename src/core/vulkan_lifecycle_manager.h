#pragma once
#include "render_frame_snapshot.h"
#include <vulkan/vulkan.h>
#include <mutex>
#include <atomic>
#include <string>

namespace vrinject {
namespace vulkan {

class VulkanLifecycleManager {
public:
    static VulkanLifecycleManager& Get();

    void OnInstanceCreated(VkInstance instance, uint32_t apiVersion);
    void OnInstanceDestroyed(VkInstance instance);

    void OnDeviceCreated(VkPhysicalDevice physicalDevice, VkDevice device, const VkDeviceCreateInfo* pCreateInfo);
    void OnDeviceDestroyed(VkDevice device);

    void OnSwapchainCreated(VkDevice device, VkSwapchainKHR swapchain, const VkSwapchainCreateInfoKHR* pCreateInfo);
    void OnSwapchainDestroyed(VkDevice device, VkSwapchainKHR swapchain);

    void OnSurfaceDestroyed(VkInstance instance, VkSurfaceKHR surface);

    // Call from ProcessPresentVulkan or vkQueuePresentKHR hook
    RenderFrameSnapshot CreateSnapshot(VkQueue presentQueue);

    void ReportDeviceLost();

    RenderState GetState() const { return m_currentState; }
    VkDevice GetCurrentDevice() const { return m_currentDevice; }

private:
    VulkanLifecycleManager() = default;

    void SetState(RenderState newState);

    mutable std::mutex m_mutex;
    RenderState m_currentState = RenderState::UNINITIALIZED;

    VkInstance m_currentInstance = nullptr;
    VkPhysicalDevice m_currentPhysicalDevice = nullptr;
    VkDevice m_currentDevice = nullptr;
    VkSwapchainKHR m_currentSwapchain = nullptr;
    VkSurfaceKHR m_currentSurface = nullptr;

    // Loader and device metadata
    uint32_t m_apiVersion = 0;
    uint32_t m_driverVersion = 0;
    bool m_supportsTimelineSemaphore = false;

    // Generators
    uint64_t m_deviceGeneration = 0;
    uint64_t m_swapchainGeneration = 0;
    uint64_t m_surfaceGeneration = 0;
    uint64_t m_queueGeneration = 0;

    // Swapchain metadata
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    VkFormat m_format = VK_FORMAT_UNDEFINED;
};

} // namespace vulkan
} // namespace vrinject
