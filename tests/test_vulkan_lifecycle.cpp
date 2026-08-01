#include <gtest/gtest.h>
#include "../src/core/vulkan_lifecycle_manager.h"
#include "../src/core/vulkan_queue_manager.h"
#include "../src/core/vulkan_dispatch_table.h"
#include "../src/core/render_frame_snapshot.h"
#include "../src/core/vulkan_snapshot_validator.h"

using namespace vrinject;
using namespace vrinject::vulkan;

class VulkanLifecycleTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset state
    }
};

TEST_F(VulkanLifecycleTest, TestLifecycleTransitions) {
    auto& lm = VulkanLifecycleManager::Get();
    
    // Simulate instance create
    VkInstance dummyInstance = reinterpret_cast<VkInstance>(0x1234);
    lm.OnInstanceCreated(dummyInstance, VK_API_VERSION_1_2);
    EXPECT_EQ(lm.GetState(), RenderState::INITIALIZING);

    // Simulate device create
    VkPhysicalDevice dummyPD = reinterpret_cast<VkPhysicalDevice>(0x5678);
    VkDevice dummyDevice = reinterpret_cast<VkDevice>(0x9ABC);
    lm.OnDeviceCreated(dummyPD, dummyDevice, nullptr);
    EXPECT_EQ(lm.GetState(), RenderState::INITIALIZING);

    // Simulate swapchain
    VkSwapchainKHR dummySwapchain = reinterpret_cast<VkSwapchainKHR>(0xDEF0);
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.surface = reinterpret_cast<VkSurfaceKHR>(0x9999);
    lm.OnSwapchainCreated(dummyDevice, dummySwapchain, &createInfo);
    EXPECT_EQ(lm.GetState(), RenderState::RUNNING);

    // Test snapshot
    VkQueue dummyQueue = reinterpret_cast<VkQueue>(0x1111);
    auto snapshot = lm.CreateSnapshot(dummyQueue);
    
    std::string error;
    bool valid = VulkanSnapshotValidator::Validate(snapshot, error);
    EXPECT_TRUE(valid) << error;
    EXPECT_EQ(snapshot.nativeSurface, createInfo.surface);

    // Destroy
    lm.OnSwapchainDestroyed(dummyDevice, dummySwapchain);
    EXPECT_EQ(lm.GetState(), RenderState::REBUILDING_RESOURCES);

    lm.OnDeviceDestroyed(dummyDevice);
    EXPECT_EQ(lm.GetState(), RenderState::DEVICE_REMOVED);
}
