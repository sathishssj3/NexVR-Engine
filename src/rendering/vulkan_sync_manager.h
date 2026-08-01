#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <mutex>

namespace vrinject {
namespace vulkan {

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

enum class VulkanSemaphoreMode {
    Binary,
    Timeline
};

class VulkanSyncManager {
public:
    VulkanSyncManager(VkDevice device);
    ~VulkanSyncManager();

    bool Initialize();
    void Destroy();

    bool WaitForFrame(uint32_t frameIndex, uint64_t timeout = UINT64_MAX);
    bool ResetFrame(uint32_t frameIndex);

    VkFence GetFence(uint32_t frameIndex) const;
    VkSemaphore GetRenderFinishedSemaphore(uint32_t frameIndex) const;
    VulkanSemaphoreMode GetSemaphoreMode() const { return m_semaphoreMode; }
    bool TimelineSemaphoresEnabled() const { return m_semaphoreMode == VulkanSemaphoreMode::Timeline; }

    static bool SupportsTimelineSemaphores(uint32_t apiVersion,
                                           const std::vector<const char*>& enabledExtensions);

private:
    VkDevice m_device = VK_NULL_HANDLE;
    std::mutex m_mutex;

    std::vector<VkFence> m_inFlightFences;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    VulkanSemaphoreMode m_semaphoreMode = VulkanSemaphoreMode::Binary;
};

} // namespace vulkan
} // namespace vrinject
