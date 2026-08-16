#include <gtest/gtest.h>
#include "rendering/vulkan/vulkan_lifecycle_manager.h"
#include "rendering/vulkan/vulkan_dispatch_table.h"
#include "hooks/vulkan_hook.h"

using namespace vrinject;
using namespace vrinject::vulkan;

class VulkanFailureTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset state
    }
};

TEST_F(VulkanFailureTest, DeviceLostTransition) {
    auto& lm = VulkanLifecycleManager::Get();
    
    VkPhysicalDevice dummyPD = reinterpret_cast<VkPhysicalDevice>(0x1);
    VkDevice dummyDevice = reinterpret_cast<VkDevice>(0x2);
    lm.OnDeviceCreated(dummyPD, dummyDevice, nullptr);
    
    EXPECT_EQ(lm.GetState(), RenderState::INITIALIZING);
    
    // Simulate VK_ERROR_DEVICE_LOST from acquire
    lm.ReportDeviceLost();
    EXPECT_EQ(lm.GetState(), RenderState::DEVICE_REMOVED);
}

TEST_F(VulkanFailureTest, RapidSwapchainRecreation) {
    auto& lm = VulkanLifecycleManager::Get();
    
    VkDevice dummyDevice = reinterpret_cast<VkDevice>(0x2);
    VkSwapchainKHR sc1 = reinterpret_cast<VkSwapchainKHR>(0x3);
    VkSwapchainKHR sc2 = reinterpret_cast<VkSwapchainKHR>(0x4);
    
    // Fast resize loop
    lm.OnSwapchainCreated(dummyDevice, sc1, nullptr);
    EXPECT_EQ(lm.GetState(), RenderState::RUNNING);
    
    lm.OnSwapchainDestroyed(dummyDevice, sc1);
    EXPECT_EQ(lm.GetState(), RenderState::REBUILDING_RESOURCES);
    
    lm.OnSwapchainCreated(dummyDevice, sc2, nullptr);
    EXPECT_EQ(lm.GetState(), RenderState::RUNNING);
}
