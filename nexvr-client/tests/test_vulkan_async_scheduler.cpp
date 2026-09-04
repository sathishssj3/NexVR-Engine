#include <gtest/gtest.h>

#include "rendering/vulkan_async_scheduler.h"

using namespace vrinject::vulkan;

TEST(VulkanAsyncScheduler, SelectsDedicatedComputeQueueWhenAvailable) {
    std::vector<VkQueueFamilyProperties> families(2);
    families[0].queueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
    families[1].queueFlags = VK_QUEUE_COMPUTE_BIT;

    auto config = VulkanAsyncScheduler::SelectQueueFamily(families, 0);

    EXPECT_TRUE(config.asyncComputeAvailable);
    EXPECT_EQ(config.selectedQueueFamily, 1u);
}

TEST(VulkanAsyncScheduler, FallsBackToGraphicsQueueWithoutDedicatedCompute) {
    std::vector<VkQueueFamilyProperties> families(1);
    families[0].queueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;

    auto config = VulkanAsyncScheduler::SelectQueueFamily(families, 0);

    EXPECT_FALSE(config.asyncComputeAvailable);
    EXPECT_EQ(config.selectedQueueFamily, 0u);
}
