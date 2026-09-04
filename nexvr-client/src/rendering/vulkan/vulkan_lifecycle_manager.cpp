#include "rendering/vulkan/vulkan_lifecycle_manager.h"
#include "rendering/vulkan/vulkan_queue_manager.h"
#include "rendering/vulkan/vulkan_dispatch_table.h"
#include "core/logger.h"
#include <iostream>

namespace vrinject {
namespace vulkan {

VulkanLifecycleManager& VulkanLifecycleManager::Get() {
    static VulkanLifecycleManager instance;
    return instance;
}

void VulkanLifecycleManager::SetState(RenderState newState) {
    if (m_currentState != newState) {
        std::cout << "[VK] Lifecycle Transition: " << (int)m_currentState << " -> " << (int)newState << std::endl;
        m_currentState = newState;
    }
}

void VulkanLifecycleManager::OnInstanceCreated(VkInstance instance, uint32_t apiVersion) {
    std::lock_guard lock(m_mutex);
    m_currentInstance = instance;
    m_apiVersion = apiVersion;
    SetState(RenderState::INITIALIZING);
}

void VulkanLifecycleManager::OnInstanceDestroyed(VkInstance instance) {
    std::lock_guard lock(m_mutex);
    if (m_currentInstance == instance) {
        m_currentInstance = nullptr;
        SetState(RenderState::SHUTTING_DOWN);
    }
}

void VulkanLifecycleManager::OnDeviceCreated(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, const VkDeviceCreateInfo* pCreateInfo) {
    std::lock_guard lock(m_mutex);
    m_currentInstance = instance;
    m_currentPhysicalDevice = physicalDevice;
    m_currentDevice = device;
    m_deviceGeneration++;

    VulkanQueueManager::Get().Initialize(physicalDevice, device);

    // Detect timeline semaphore extension if present in pCreateInfo
    m_supportsTimelineSemaphore = false;
    if (pCreateInfo) {
        for (uint32_t i = 0; i < pCreateInfo->enabledExtensionCount; ++i) {
            if (strcmp(pCreateInfo->ppEnabledExtensionNames[i], VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME) == 0) {
                m_supportsTimelineSemaphore = true;
                break;
            }
        }
    }

    SetState(RenderState::INITIALIZING);
}

void VulkanLifecycleManager::OnDeviceDestroyed(VkDevice device) {
    std::lock_guard lock(m_mutex);
    if (m_currentDevice == device) {
        m_currentDevice = nullptr;
        m_currentPhysicalDevice = nullptr;
        m_deviceGeneration++;

        VulkanQueueManager::Get().Shutdown();
        SetState(RenderState::DEVICE_REMOVED); // or SHUTTING_DOWN
    }
}

void VulkanLifecycleManager::OnSwapchainCreated(VkDevice device, VkSwapchainKHR swapchain, const VkSwapchainCreateInfoKHR* pCreateInfo) {
    std::lock_guard lock(m_mutex);
    if (m_currentDevice != device) {
        LOG_WARN("VulkanLifecycleManager: OnSwapchainCreated called with device %p, expected %p", device, m_currentDevice);
        return;
    }
    
    LOG_INFO("VulkanLifecycleManager: OnSwapchainCreated setting RUNNING state. Swapchain=%p", swapchain);
    m_currentSwapchain = swapchain;
    m_swapchainGeneration++;
    m_queueGeneration++;

    if (pCreateInfo) {
        if (m_currentSurface != pCreateInfo->surface) {
            m_currentSurface = pCreateInfo->surface;
            m_surfaceGeneration++;
        }
        m_width = pCreateInfo->imageExtent.width;
        m_height = pCreateInfo->imageExtent.height;
        m_format = pCreateInfo->imageFormat;
    }

    SetState(RenderState::RUNNING);
}

void VulkanLifecycleManager::OnSwapchainDestroyed(VkDevice device, VkSwapchainKHR swapchain) {
    std::lock_guard lock(m_mutex);
    if (m_currentSwapchain == swapchain) {
        m_currentSwapchain = nullptr;
        if (m_currentState == RenderState::RUNNING) {
            SetState(RenderState::REBUILDING_RESOURCES);
        }
    }
}

void VulkanLifecycleManager::OnSurfaceDestroyed(VkInstance instance, VkSurfaceKHR surface) {
    std::lock_guard lock(m_mutex);
    if (m_currentSurface == surface) {
        m_currentSurface = nullptr;
        m_surfaceGeneration++;
    }
}

RenderFrameSnapshot VulkanLifecycleManager::CreateSnapshot(VkQueue presentQueue) {
    std::lock_guard lock(m_mutex);
    RenderFrameSnapshot snapshot;
    snapshot.backend = GraphicsBackend::Vulkan;
    snapshot.state = m_currentState;
    snapshot.nativeDevice = m_currentDevice;
    snapshot.nativeContext = presentQueue;
    snapshot.nativeSwapchain = m_currentSwapchain;
    snapshot.nativeSurface = m_currentSurface;
    snapshot.nativeInstance = m_currentInstance;
    snapshot.nativePhysicalDevice = m_currentPhysicalDevice;
    snapshot.width = m_width;
    snapshot.height = m_height;
    snapshot.format = m_format;
    snapshot.apiVersion = m_apiVersion;
    snapshot.driverVersion = m_driverVersion;
    
    snapshot.epoch.deviceGeneration = m_deviceGeneration;
    snapshot.epoch.swapchainGeneration = m_swapchainGeneration;
    snapshot.epoch.surfaceGeneration = m_surfaceGeneration;
    snapshot.epoch.queueGeneration = m_queueGeneration;
    
    return snapshot;
}

void VulkanLifecycleManager::ReportDeviceLost() {
    std::lock_guard lock(m_mutex);
    SetState(RenderState::DEVICE_REMOVED);
}

} // namespace vulkan
} // namespace vrinject
