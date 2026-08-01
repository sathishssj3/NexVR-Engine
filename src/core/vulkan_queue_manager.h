#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <mutex>

namespace vrinject {
namespace vulkan {

struct QueueRecord {
    uint32_t queueFamilyIndex = 0;
    uint32_t queueIndex = 0;
    VkQueue queueHandle = nullptr;
    VkQueueFlags capabilities = 0;

    bool supportsPresent = false;
    bool supportsGraphics = false;
    bool supportsCompute = false;
    bool supportsTransfer = false;
};

class VulkanQueueManager {
public:
    static VulkanQueueManager& Get();

    void Initialize(VkPhysicalDevice physicalDevice, VkDevice device);
    void Shutdown();

    // The hooked vkGetDeviceQueue calls this
    void RegisterQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue queue);

    // Update present support dynamically if queried
    void MarkQueueFamilyPresentSupport(uint32_t queueFamilyIndex, bool supportsPresent);

    VkQueue GetMainGraphicsQueue() const;
    const QueueRecord* GetQueueRecord(VkQueue queue) const;
    std::vector<QueueRecord> GetQueues() const { return m_knownQueues; }
    VkDevice GetDevice() const { return m_device; }

private:
    VulkanQueueManager() = default;

    mutable std::mutex m_mutex;
    VkPhysicalDevice m_physicalDevice = nullptr;
    VkDevice m_device = nullptr;

    std::vector<VkQueueFamilyProperties> m_queueFamilyProperties;
    std::vector<QueueRecord> m_knownQueues;

    VkQueue m_cachedMainQueue = nullptr;

    void ReevaluateMainQueue();
};

} // namespace vulkan
} // namespace vrinject
