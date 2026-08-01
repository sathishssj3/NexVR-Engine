#include <gtest/gtest.h>

#include "src/rendering/vulkan_sync_manager.h"

using namespace vrinject::vulkan;

TEST(VulkanTimelineSemaphore, EnablesForVulkan12) {
    EXPECT_TRUE(VulkanSyncManager::SupportsTimelineSemaphores(VK_API_VERSION_1_2, {}));
}

TEST(VulkanTimelineSemaphore, EnablesForKhrExtensionOnVulkan11) {
    EXPECT_TRUE(VulkanSyncManager::SupportsTimelineSemaphores(
        VK_API_VERSION_1_1,
        {VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME}));
}

TEST(VulkanTimelineSemaphore, FallsBackWithoutSupport) {
    EXPECT_FALSE(VulkanSyncManager::SupportsTimelineSemaphores(VK_API_VERSION_1_1, {}));
}
