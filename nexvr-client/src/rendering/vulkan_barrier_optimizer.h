#pragma once

#include <vulkan/vulkan.h>

#include <cstddef>
#include <vector>

namespace vrinject {
namespace vulkan {

struct BarrierOptimizationResult {
    std::vector<VkImageMemoryBarrier> barriers;
    size_t inputCount = 0;
    size_t removedCount = 0;
};

class VulkanBarrierOptimizer {
public:
    static BarrierOptimizationResult OptimizeImageBarriers(const std::vector<VkImageMemoryBarrier>& input);
};

} // namespace vulkan
} // namespace vrinject
