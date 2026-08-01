#include "vulkan_command_manager.h"
#include "../core/vulkan_dispatch_table.h"

namespace vrinject {
namespace vulkan {

VulkanCommandManager::VulkanCommandManager(VkDevice device, uint32_t queueFamilyIndex) 
    : m_device(device), m_queueFamilyIndex(queueFamilyIndex) {
}

VulkanCommandManager::~VulkanCommandManager() {
    Destroy();
}

bool VulkanCommandManager::Initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(m_device);
    if (!dt) return false;

    m_commandPools.resize(MAX_FRAMES_IN_FLIGHT, VK_NULL_HANDLE);
    m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT, VK_NULL_HANDLE);
    m_stats = {};

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT; // We reset the pool every frame
    poolInfo.queueFamilyIndex = m_queueFamilyIndex;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (dt->CreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPools[i]) != VK_SUCCESS) {
            return false;
        }

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_commandPools[i];
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        if (dt->AllocateCommandBuffers(m_device, &allocInfo, &m_commandBuffers[i]) != VK_SUCCESS) {
            return false;
        }
        m_stats.allocatedCommandBuffers++;
    }

    return true;
}

void VulkanCommandManager::Destroy() {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(m_device);
    if (!dt) return;

    for (size_t i = 0; i < m_commandPools.size(); i++) {
        if (m_commandPools[i]) {
            dt->DestroyCommandPool(m_device, m_commandPools[i], nullptr);
        }
    }
    m_commandPools.clear();
    m_commandBuffers.clear();
    m_stats = {};
}

VkCommandBuffer VulkanCommandManager::BeginFrame(uint32_t frameIndex) {
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(m_device);
    if (!dt || frameIndex >= m_commandPools.size()) return VK_NULL_HANDLE;

    // Reset the pool (safe because we assume the SyncManager already waited for this frame's fence)
    dt->ResetCommandPool(m_device, m_commandPools[frameIndex], 0);
    m_stats.resetCount++;
    m_stats.reuseCount++;

    VkCommandBuffer cmd = m_commandBuffers[frameIndex];
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (dt->BeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    return cmd;
}

bool VulkanCommandManager::EndFrame(uint32_t frameIndex) {
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(m_device);
    if (!dt || frameIndex >= m_commandBuffers.size()) return false;

    return dt->EndCommandBuffer(m_commandBuffers[frameIndex]) == VK_SUCCESS;
}

VkCommandBuffer VulkanCommandManager::GetCommandBuffer(uint32_t frameIndex) const {
    if (frameIndex < m_commandBuffers.size()) {
        return m_commandBuffers[frameIndex];
    }
    return VK_NULL_HANDLE;
}

} // namespace vulkan
} // namespace vrinject
