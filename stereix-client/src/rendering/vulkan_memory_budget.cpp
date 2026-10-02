#include "rendering/vulkan_memory_budget.h"

namespace vrinject {
namespace vulkan {

void VulkanMemoryBudget::UpdateTrackedResources(uint64_t eyeTextureBytes,
                                                uint64_t temporaryBufferBytes,
                                                uint64_t pipelineCacheBytes,
                                                uint64_t descriptorPoolBytes) {
    m_snapshot.eyeTextureBytes = eyeTextureBytes;
    m_snapshot.temporaryBufferBytes = temporaryBufferBytes;
    m_snapshot.pipelineCacheBytes = pipelineCacheBytes;
    m_snapshot.descriptorPoolBytes = descriptorPoolBytes;
    m_snapshot.totalTrackedBytes = eyeTextureBytes + temporaryBufferBytes + pipelineCacheBytes + descriptorPoolBytes + m_snapshot.aiModelBytes + m_snapshot.aiTensorBytes;
}

void VulkanMemoryBudget::UpdateAiBudget(uint64_t modelBytes, uint64_t tensorBytes) {
    m_snapshot.aiModelBytes = modelBytes;
    m_snapshot.aiTensorBytes = tensorBytes;
    m_snapshot.totalTrackedBytes = m_snapshot.eyeTextureBytes + m_snapshot.temporaryBufferBytes + 
                                   m_snapshot.pipelineCacheBytes + m_snapshot.descriptorPoolBytes + 
                                   modelBytes + tensorBytes;
}

void VulkanMemoryBudget::UpdateDeviceBudget(uint64_t budgetBytes, uint64_t usageBytes) {
    m_snapshot.budgetBytes = budgetBytes;
    m_snapshot.usageBytes = usageBytes;
}

} // namespace vulkan
} // namespace vrinject
