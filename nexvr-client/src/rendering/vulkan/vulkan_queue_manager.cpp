#include "rendering/vulkan/vulkan_queue_manager.h"
#include "rendering/vulkan/vulkan_dispatch_table.h"
#include <iostream>

namespace vrinject {
namespace vulkan {

VulkanQueueManager& VulkanQueueManager::Get() {
    static VulkanQueueManager instance;
    return instance;
}

void VulkanQueueManager::Initialize(VkPhysicalDevice physicalDevice, VkDevice device) {
    std::lock_guard lock(m_mutex);
    m_physicalDevice = physicalDevice;
    m_device = device;
    m_knownQueues.clear();
    m_cachedMainQueue = nullptr;

    const auto* instanceDt = VulkanDispatchTable::Get().GetInstanceDispatch(nullptr); // Needs instance, but GetPhysicalDeviceProperties is instance level.
    // For now we assume the instance dispatch table has GetPhysicalDeviceProperties globally accessible or we find it.
    // Wait, queue properties are from vkGetPhysicalDeviceQueueFamilyProperties. Let's just hook or retrieve it.
    // Actually, we don't strictly need it right now if we trust what the app requests, but it's better to have.
    // We will just fetch it later or assume graphics capability if not available.
}

void VulkanQueueManager::Shutdown() {
    std::lock_guard lock(m_mutex);
    m_device = nullptr;
    m_physicalDevice = nullptr;
    m_knownQueues.clear();
    m_cachedMainQueue = nullptr;
}

void VulkanQueueManager::RegisterQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue queue) {
    std::lock_guard lock(m_mutex);
    if (device != m_device) return;

    for (auto& q : m_knownQueues) {
        if (q.queueHandle == queue) return; // Already registered
    }

    QueueRecord record;
    record.queueFamilyIndex = queueFamilyIndex;
    record.queueIndex = queueIndex;
    record.queueHandle = queue;
    
    // Default assumption, ideally we query VkQueueFamilyProperties
    record.supportsGraphics = true;
    record.supportsPresent = true;
    record.supportsCompute = true;
    record.supportsTransfer = true;

    m_knownQueues.push_back(record);
    ReevaluateMainQueue();
}

void VulkanQueueManager::MarkQueueFamilyPresentSupport(uint32_t queueFamilyIndex, bool supportsPresent) {
    std::lock_guard lock(m_mutex);
    for (auto& q : m_knownQueues) {
        if (q.queueFamilyIndex == queueFamilyIndex) {
            q.supportsPresent = supportsPresent;
        }
    }
    ReevaluateMainQueue();
}

VkQueue VulkanQueueManager::GetMainGraphicsQueue() const {
    std::lock_guard lock(m_mutex); // Not ideal for hot path, but fine for now
    return m_cachedMainQueue;
}

const QueueRecord* VulkanQueueManager::GetQueueRecord(VkQueue queue) const {
    std::lock_guard lock(m_mutex);
    for (const auto& q : m_knownQueues) {
        if (q.queueHandle == queue) return &q;
    }
    return nullptr;
}

void VulkanQueueManager::ReevaluateMainQueue() {
    // Basic heuristic: find first queue supporting graphics and present
    for (const auto& q : m_knownQueues) {
        if (q.supportsGraphics && q.supportsPresent) {
            m_cachedMainQueue = q.queueHandle;
            return;
        }
    }
    
    // Fallback: any graphics queue
    for (const auto& q : m_knownQueues) {
        if (q.supportsGraphics) {
            m_cachedMainQueue = q.queueHandle;
            return;
        }
    }
}

} // namespace vulkan
} // namespace vrinject
