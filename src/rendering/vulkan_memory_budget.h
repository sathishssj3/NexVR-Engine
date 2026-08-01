#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

namespace vrinject {
namespace vulkan {

struct VulkanMemoryBudgetSnapshot {
    uint64_t eyeTextureBytes = 0;
    uint64_t temporaryBufferBytes = 0;
    uint64_t pipelineCacheBytes = 0;
    uint64_t descriptorPoolBytes = 0;
    uint64_t totalTrackedBytes = 0;
    uint64_t budgetBytes = 0;
    uint64_t usageBytes = 0;
    bool extMemoryBudgetAvailable = false;
};

class VulkanMemoryBudget {
public:
    void SetExtensionAvailable(bool available) { m_snapshot.extMemoryBudgetAvailable = available; }
    void UpdateTrackedResources(uint64_t eyeTextureBytes,
                                uint64_t temporaryBufferBytes,
                                uint64_t pipelineCacheBytes,
                                uint64_t descriptorPoolBytes);
    void UpdateDeviceBudget(uint64_t budgetBytes, uint64_t usageBytes);
    VulkanMemoryBudgetSnapshot GetSnapshot() const { return m_snapshot; }

private:
    VulkanMemoryBudgetSnapshot m_snapshot{};
};

} // namespace vulkan
} // namespace vrinject
