#include "vulkan_memory_budget.h"

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
    m_snapshot.totalTrackedBytes =
        eyeTextureBytes + temporaryBufferBytes + pipelineCacheBytes + descriptorPoolBytes;
}

void VulkanMemoryBudget::UpdateDeviceBudget(uint64_t budgetBytes, uint64_t usageBytes) {
    m_snapshot.budgetBytes = budgetBytes;
    m_snapshot.usageBytes = usageBytes;
}

} // namespace vulkan
} // namespace vrinject
