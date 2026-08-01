#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <mutex>
#include "vulkan_sync_manager.h" // For MAX_FRAMES_IN_FLIGHT

namespace vrinject {
namespace vulkan {

class VulkanCommandManager {
public:
    VulkanCommandManager(VkDevice device, uint32_t queueFamilyIndex);
    ~VulkanCommandManager();

    bool Initialize();
    void Destroy();

    VkCommandBuffer BeginFrame(uint32_t frameIndex);
    bool EndFrame(uint32_t frameIndex);

    VkCommandBuffer GetCommandBuffer(uint32_t frameIndex) const;

private:
    VkDevice m_device = VK_NULL_HANDLE;
    uint32_t m_queueFamilyIndex = 0;
    std::mutex m_mutex;

    std::vector<VkCommandPool> m_commandPools;
    std::vector<VkCommandBuffer> m_commandBuffers;
};

} // namespace vulkan
} // namespace vrinject
