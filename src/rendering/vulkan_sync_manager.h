#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <mutex>

namespace vrinject {
namespace vulkan {

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

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

private:
    VkDevice m_device = VK_NULL_HANDLE;
    std::mutex m_mutex;

    std::vector<VkFence> m_inFlightFences;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
};

} // namespace vulkan
} // namespace vrinject
