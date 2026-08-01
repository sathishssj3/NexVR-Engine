#include "vulkan_barrier_optimizer.h"

namespace vrinject {
namespace vulkan {
namespace {

bool SameImageRange(const VkImageMemoryBarrier& lhs, const VkImageMemoryBarrier& rhs) {
    const auto& a = lhs.subresourceRange;
    const auto& b = rhs.subresourceRange;
    return lhs.image == rhs.image &&
           a.aspectMask == b.aspectMask &&
           a.baseMipLevel == b.baseMipLevel &&
           a.levelCount == b.levelCount &&
           a.baseArrayLayer == b.baseArrayLayer &&
           a.layerCount == b.layerCount;
}

bool IsNoOp(const VkImageMemoryBarrier& barrier) {
    return barrier.oldLayout == barrier.newLayout &&
           barrier.srcAccessMask == barrier.dstAccessMask &&
           barrier.srcQueueFamilyIndex == barrier.dstQueueFamilyIndex;
}

} // namespace

BarrierOptimizationResult VulkanBarrierOptimizer::OptimizeImageBarriers(
    const std::vector<VkImageMemoryBarrier>& input) {
    BarrierOptimizationResult result{};
    result.inputCount = input.size();

    for (const auto& barrier : input) {
        if (IsNoOp(barrier)) {
            result.removedCount++;
            continue;
        }

        bool replaced = false;
        for (auto& existing : result.barriers) {
            if (SameImageRange(existing, barrier)) {
                existing.newLayout = barrier.newLayout;
                existing.dstAccessMask = barrier.dstAccessMask;
                existing.dstQueueFamilyIndex = barrier.dstQueueFamilyIndex;
                replaced = true;
                result.removedCount++;
                break;
            }
        }

        if (!replaced) {
            result.barriers.push_back(barrier);
        }
    }

    return result;
}

} // namespace vulkan
} // namespace vrinject
