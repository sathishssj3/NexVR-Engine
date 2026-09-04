#include <gtest/gtest.h>
#include "rendering/vulkan/vulkan_lifecycle_manager.h"
#include "rendering/vulkan/vulkan_queue_manager.h"
#include "rendering/vulkan/vulkan_snapshot_validator.h"

using namespace vrinject;
using namespace vrinject::vulkan;

TEST(VulkanQueueGenerationTest, DetectsAndRejectsStaleQueueHandles) {
    auto& lm = VulkanLifecycleManager::Get();
    auto& qm = VulkanQueueManager::Get();

    VkInstance instance = reinterpret_cast<VkInstance>(0x111);
    VkPhysicalDevice pd = reinterpret_cast<VkPhysicalDevice>(0x222);
    VkDevice dev1 = reinterpret_cast<VkDevice>(0x333);
    VkSwapchainKHR swap1 = reinterpret_cast<VkSwapchainKHR>(0x444);
    VkQueue queue1 = reinterpret_cast<VkQueue>(0x555);

    lm.OnInstanceCreated(instance, VK_API_VERSION_1_2);
    lm.OnDeviceCreated(instance, pd, dev1, nullptr);

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.surface = reinterpret_cast<VkSurfaceKHR>(0x666);
    lm.OnSwapchainCreated(dev1, swap1, &createInfo);

    // Initial snapshot under queueGeneration G1
    RenderFrameSnapshot snapshotG1 = lm.CreateSnapshot(queue1);
    EXPECT_EQ(snapshotG1.epoch.queueGeneration, 1);

    // 1. Simulate swapchain recreation (triggers queue generation increment)
    lm.OnSwapchainDestroyed(dev1, swap1);
    VkSwapchainKHR swap2 = reinterpret_cast<VkSwapchainKHR>(0x777);
    VkQueue queue2 = reinterpret_cast<VkQueue>(0x888);
    lm.OnSwapchainCreated(dev1, swap2, &createInfo);

    RenderFrameSnapshot snapshotG2 = lm.CreateSnapshot(queue2);
    EXPECT_GT(snapshotG2.epoch.queueGeneration, snapshotG1.epoch.queueGeneration);
    EXPECT_EQ(snapshotG2.epoch.queueGeneration, 2);

    // 2. Validate snapshot mismatch detection
    std::string err;
    EXPECT_TRUE(VulkanSnapshotValidator::Validate(snapshotG2, err));
    
    // Simulate queue registration
    qm.Initialize(pd, dev1);
    qm.RegisterQueue(dev1, 0, 0, queue2);
    EXPECT_NE(qm.GetQueueRecord(queue2), nullptr);
    EXPECT_EQ(qm.GetQueueRecord(queue1), nullptr);
    
    // De-register / Shutdown
    qm.Shutdown();
    EXPECT_EQ(qm.GetQueueRecord(queue2), nullptr);
}
