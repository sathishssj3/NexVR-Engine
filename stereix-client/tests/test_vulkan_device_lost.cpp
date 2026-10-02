#include <gtest/gtest.h>
#include "rendering/vulkan/vulkan_lifecycle_manager.h"
#include "rendering/vulkan/vulkan_snapshot_validator.h"
#include "core/fault_injector.h"
#include "rendering/vulkan/vulkan_graphics_backend.h"

using namespace vrinject;
using namespace vrinject::vulkan;

class VulkanDeviceLostTest : public ::testing::Test {
protected:
    void SetUp() override {
        FaultInjector::Clear();
    }
    void TearDown() override {
        FaultInjector::Clear();
    }
};

TEST_F(VulkanDeviceLostTest, DeviceLostTransitionAndRecovery) {
    auto& lm = VulkanLifecycleManager::Get();
    
    VkInstance dummyInstance = reinterpret_cast<VkInstance>(0x1001);
    VkPhysicalDevice dummyPD = reinterpret_cast<VkPhysicalDevice>(0x2002);
    VkDevice dummyDevice = reinterpret_cast<VkDevice>(0x3003);
    VkSwapchainKHR dummySwapchain = reinterpret_cast<VkSwapchainKHR>(0x4004);
    VkQueue dummyQueue = reinterpret_cast<VkQueue>(0x5005);
    
    lm.OnInstanceCreated(dummyInstance, VK_API_VERSION_1_2);
    lm.OnDeviceCreated(dummyInstance, dummyPD, dummyDevice, nullptr);
    
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.surface = reinterpret_cast<VkSurfaceKHR>(0x6006);
    lm.OnSwapchainCreated(dummyDevice, dummySwapchain, &createInfo);
    
    EXPECT_EQ(lm.GetState(), RenderState::RUNNING);

    // 1. Simulate Device Loss Fault Injection
    FaultInjector::InjectVulkanFault(VulkanFaultPoint::QUEUE_SUBMIT_DEVICE_LOST);
    if (FaultInjector::ShouldVulkanFail(VulkanFaultPoint::QUEUE_SUBMIT_DEVICE_LOST)) {
        lm.ReportDeviceLost();
    }
    
    EXPECT_EQ(lm.GetState(), RenderState::DEVICE_REMOVED);

    // Verify snapshot under DEVICE_REMOVED is rejected by validator
    auto snapshot = lm.CreateSnapshot(dummyQueue);
    std::string err;
    EXPECT_FALSE(VulkanSnapshotValidator::Validate(snapshot, err));
    EXPECT_NE(err.find("DEVICE_REMOVED"), std::string::npos);

    // 2. Simulate Device Recovery Sequence
    lm.OnDeviceDestroyed(dummyDevice);
    VkDevice newDevice = reinterpret_cast<VkDevice>(0x7007);
    VkSwapchainKHR newSwapchain = reinterpret_cast<VkSwapchainKHR>(0x8008);
    
    lm.OnDeviceCreated(dummyInstance, dummyPD, newDevice, nullptr);
    lm.OnSwapchainCreated(newDevice, newSwapchain, &createInfo);
    
    EXPECT_EQ(lm.GetState(), RenderState::RUNNING);
    auto newSnapshot = lm.CreateSnapshot(dummyQueue);
    EXPECT_TRUE(VulkanSnapshotValidator::Validate(newSnapshot, err));
    EXPECT_GT(newSnapshot.epoch.deviceGeneration, snapshot.epoch.deviceGeneration);
}
