#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <mutex>
#include "rendering/vulkan_sync_manager.h" // For MAX_FRAMES_IN_FLIGHT

namespace vrinject {
namespace vulkan {

struct VulkanCommandRecycleStats {
    uint32_t allocatedCommandBuffers = 0;
    uint64_t resetCount = 0;
    uint64_t reuseCount = 0;
};

class VulkanCommandManager {
public:
    VulkanCommandManager(VkDevice device, uint32_t queueFamilyIndex);
    ~VulkanCommandManager();

    bool Initialize();
    void Destroy();

    VkCommandBuffer BeginFrame(uint32_t frameIndex);
    bool EndFrame(uint32_t frameIndex);

    VkCommandBuffer GetCommandBuffer(uint32_t frameIndex) const;
    VulkanCommandRecycleStats GetRecycleStats() const { return m_stats; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    uint32_t m_queueFamilyIndex = 0;
    std::mutex m_mutex;

    std::vector<VkCommandPool> m_commandPools;
    std::vector<VkCommandBuffer> m_commandBuffers;
    VulkanCommandRecycleStats m_stats{};
};

} // namespace vulkan
} // namespace vrinject
