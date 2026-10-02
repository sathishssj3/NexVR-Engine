#include <gtest/gtest.h>

#include "rendering/vulkan_barrier_optimizer.h"

using namespace vrinject::vulkan;

TEST(VulkanBarrierOptimizer, RemovesNoOpBarriers) {
    VkImageMemoryBarrier barrier{};
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

    auto result = VulkanBarrierOptimizer::OptimizeImageBarriers({barrier});

    EXPECT_EQ(result.inputCount, 1u);
    EXPECT_EQ(result.removedCount, 1u);
    EXPECT_TRUE(result.barriers.empty());
}

TEST(VulkanBarrierOptimizer, MergesRepeatedImageTransitions) {
    VkImageMemoryBarrier first{};
    first.image = VK_NULL_HANDLE;
    first.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    first.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    first.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    first.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    first.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    first.subresourceRange.levelCount = 1;
    first.subresourceRange.layerCount = 1;

    VkImageMemoryBarrier second = first;
    second.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    second.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    auto result = VulkanBarrierOptimizer::OptimizeImageBarriers({first, second});

    ASSERT_EQ(result.barriers.size(), 1u);
    EXPECT_EQ(result.removedCount, 1u);
    EXPECT_EQ(result.barriers[0].oldLayout, VK_IMAGE_LAYOUT_UNDEFINED);
    EXPECT_EQ(result.barriers[0].newLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
}
