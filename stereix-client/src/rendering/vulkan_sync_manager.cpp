#include "rendering/vulkan_sync_manager.h"
#include "rendering/vulkan/vulkan_dispatch_table.h"
#include <cstring>
#include <iostream>

namespace vrinject {
namespace vulkan {

VulkanSyncManager::VulkanSyncManager(VkDevice device) : m_device(device) {
}

bool VulkanSyncManager::SupportsTimelineSemaphores(uint32_t apiVersion,
                                                   const std::vector<const char*>& enabledExtensions) {
    if (apiVersion >= VK_API_VERSION_1_2) {
        return true;
    }
    for (const char* extension : enabledExtensions) {
        if (extension && std::strcmp(extension, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME) == 0) {
            return true;
        }
    }
    return false;
}

VulkanSyncManager::~VulkanSyncManager() {
    Destroy();
}

bool VulkanSyncManager::Initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(m_device);
    if (!dt) {
        std::cerr << "[SyncManager] Initialize: no dispatch table for device " << (void*)m_device << "\n";
        return false;
    }
    std::cerr << "[SyncManager] Initialize: device=" << (void*)m_device << "\n";

    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkResult semRes = dt->CreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]);
        VkResult fenceRes = dt->CreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]);
        std::cerr << "[SyncManager] frame " << i << " semRes=" << semRes
                  << " fenceRes=" << fenceRes
                  << " fence=" << (void*)m_inFlightFences[i] << "\n";
        if (semRes != VK_SUCCESS || fenceRes != VK_SUCCESS) {
            return false;
        }
        // Immediately verify the pre-signalled fence is waitable
        VkResult testWait = ::vkWaitForFences(m_device, 1, &m_inFlightFences[i], VK_TRUE, 0);
        std::cerr << "[SyncManager] frame " << i << " immediate-wait result=" << testWait << "\n";
    }

    return true;
}

void VulkanSyncManager::Destroy() {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(m_device);
    if (!dt) return;

    for (size_t i = 0; i < m_renderFinishedSemaphores.size(); i++) {
        dt->DestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
        dt->DestroyFence(m_device, m_inFlightFences[i], nullptr);
    }
    
    m_renderFinishedSemaphores.clear();
    m_inFlightFences.clear();
}

bool VulkanSyncManager::WaitForFrame(uint32_t frameIndex, uint64_t timeout) {
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(m_device);
    if (!dt || frameIndex >= m_inFlightFences.size()) return false;
    
    // Try direct call first to bypass any dispatch table layer issues
    VkResult res = ::vkWaitForFences(m_device, 1, &m_inFlightFences[frameIndex], VK_TRUE, timeout);
    if (res != VK_SUCCESS) {
        std::cerr << "vkWaitForFences (direct) failed with code: " << res
                  << " device=" << (void*)m_device
                  << " fence=" << (void*)m_inFlightFences[frameIndex] << "\n";
        return false;
    }
    return true;
}

bool VulkanSyncManager::ResetFrame(uint32_t frameIndex) {
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(m_device);
    if (!dt || frameIndex >= m_inFlightFences.size()) return false;
    
    return dt->ResetFences(m_device, 1, &m_inFlightFences[frameIndex]) == VK_SUCCESS;
}

VkFence VulkanSyncManager::GetFence(uint32_t frameIndex) const {
    if (frameIndex < m_inFlightFences.size()) return m_inFlightFences[frameIndex];
    return VK_NULL_HANDLE;
}

VkSemaphore VulkanSyncManager::GetRenderFinishedSemaphore(uint32_t frameIndex) const {
    if (frameIndex < m_renderFinishedSemaphores.size()) return m_renderFinishedSemaphores[frameIndex];
    return VK_NULL_HANDLE;
}

} // namespace vulkan
} // namespace vrinject
