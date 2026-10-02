#pragma once

#include <vulkan/vulkan.h>
#include "heuristics/camera_snapshot.h"
#include "heuristics/depth_snapshot.h"
#include "rendering/vulkan_stereo_resource_manager.h"
#include "rendering/vulkan_pipeline_cache.h"
#include "rendering/vulkan_descriptor_manager.h"
#include "rendering/vulkan_command_manager.h"
#include "rendering/vulkan_sync_manager.h"
#include "rendering/vulkan_resource_state_tracker.h"

namespace vrinject {
namespace vulkan {

class VulkanStereoRenderer {
public:
    VulkanStereoRenderer(VkDevice device, VkQueue queue, uint32_t queueFamilyIndex);
    ~VulkanStereoRenderer();

    bool Initialize();
    uint32_t GetCurrentFrameIndex() const { return m_frameIndex; }

    // cameraBuffer + depthView are the actual Vulkan handles for descriptor binding
    bool Render(const CameraSnapshot& camera,
                const DepthSnapshot& depth,
                VkBuffer cameraBuffer,
                VkDeviceSize cameraBufferSize,
                VkImageView gameColorView,
                VkImageView depthView,
                VulkanStereoResourceManager& resourceManager,
                VulkanPipelineCache& pipelineCache,
                VulkanDescriptorManager& descriptorManager,
                VulkanCommandManager& commandManager,
                VulkanSyncManager& syncManager,
                VulkanResourceStateTracker& stateTracker,
                VkImage oxrLeftDest,
                VkImage oxrRightDest,
                bool shouldAttemptStereo,
                VkImageView uiMaskView = VK_NULL_HANDLE,
                uint32_t waitSemaphoreCount = 0,
                const VkSemaphore* pWaitSemaphores = nullptr,
                VkSemaphore signalSemaphore = VK_NULL_HANDLE);

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_queue = VK_NULL_HANDLE;
    uint32_t m_queueFamilyIndex = 0;
    uint32_t m_frameIndex = 0;
};

} // namespace vulkan
} // namespace vrinject
