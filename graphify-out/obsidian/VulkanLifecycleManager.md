---
source_file: "src/core/vulkan_lifecycle_manager.h"
type: "code"
community: "Community 12"
location: "L11"
tags:
  - graphify/code
  - graphify/EXTRACTED
  - community/Community_12
---

# VulkanLifecycleManager

## Connections
- [[dot-GetState()_1]] - `method` [EXTRACTED]
- [[dot-VulkanLifecycleManager()]] - `method` [EXTRACTED]
- [[CreateSnapshot]] - `defines` [EXTRACTED]
- [[Hooked_vkAcquireNextImageKHR()]] - `references` [EXTRACTED]
- [[Hooked_vkCreateDevice()]] - `references` [EXTRACTED]
- [[Hooked_vkCreateInstance()]] - `references` [EXTRACTED]
- [[Hooked_vkCreateSwapchainKHR()]] - `references` [EXTRACTED]
- [[Hooked_vkDestroyDevice()]] - `references` [EXTRACTED]
- [[Hooked_vkDestroyInstance()]] - `references` [EXTRACTED]
- [[Hooked_vkDestroySurfaceKHR()]] - `references` [EXTRACTED]
- [[Hooked_vkDestroySwapchainKHR()]] - `references` [EXTRACTED]
- [[Hooked_vkQueuePresentKHR()]] - `references` [EXTRACTED]
- [[OnDeviceCreated]] - `defines` [EXTRACTED]
- [[OnDeviceDestroyed]] - `defines` [EXTRACTED]
- [[OnInstanceCreated]] - `defines` [EXTRACTED]
- [[OnInstanceDestroyed]] - `defines` [EXTRACTED]
- [[OnSurfaceDestroyed]] - `defines` [EXTRACTED]
- [[OnSwapchainCreated]] - `defines` [EXTRACTED]
- [[OnSwapchainDestroyed]] - `defines` [EXTRACTED]
- [[RenderState_1]] - `references` [EXTRACTED]
- [[ReportDeviceLost]] - `defines` [EXTRACTED]
- [[SetState]] - `defines` [EXTRACTED]
- [[VkDevice_2]] - `references` [EXTRACTED]
- [[VkFormat]] - `references` [EXTRACTED]
- [[VkInstance_2]] - `references` [EXTRACTED]
- [[VkPhysicalDevice_1]] - `references` [EXTRACTED]
- [[VkSurfaceKHR_1]] - `references` [EXTRACTED]
- [[VkSwapchainKHR_2]] - `references` [EXTRACTED]
- [[m_apiVersion]] - `defines` [EXTRACTED]
- [[m_currentDevice]] - `defines` [EXTRACTED]
- [[m_currentInstance]] - `defines` [EXTRACTED]
- [[m_currentPhysicalDevice]] - `defines` [EXTRACTED]
- [[m_currentState]] - `defines` [EXTRACTED]
- [[m_currentSurface]] - `defines` [EXTRACTED]
- [[m_currentSwapchain]] - `defines` [EXTRACTED]
- [[m_deviceGeneration]] - `defines` [EXTRACTED]
- [[m_driverVersion]] - `defines` [EXTRACTED]
- [[m_format]] - `defines` [EXTRACTED]
- [[m_height]] - `defines` [EXTRACTED]
- [[m_mutex_3]] - `defines` [EXTRACTED]
- [[m_queueGeneration]] - `defines` [EXTRACTED]
- [[m_supportsTimelineSemaphore]] - `defines` [EXTRACTED]
- [[m_surfaceGeneration]] - `defines` [EXTRACTED]
- [[m_swapchainGeneration]] - `defines` [EXTRACTED]
- [[m_width]] - `defines` [EXTRACTED]
- [[mutex_]] - `references` [EXTRACTED]
- [[vulkan_lifecycle_manager.h]] - `contains` [EXTRACTED]

#graphify/code #graphify/EXTRACTED #community/Community_12