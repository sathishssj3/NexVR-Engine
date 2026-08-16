#include <gtest/gtest.h>
#include "rendering/vulkan/vulkan_resource_tracker.h"

using namespace vrinject;
using namespace vrinject::vulkan;

class VulkanResourceTrackingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset state not strictly needed for singleton, but good practice.
    }
};

TEST_F(VulkanResourceTrackingTest, ImageTracking) {
    auto& tracker = VulkanResourceTracker::Get();
    
    VkDevice dummyDevice = reinterpret_cast<VkDevice>(0x1000);
    VkImage dummyImage = reinterpret_cast<VkImage>(0x2000);
    
    VkImageCreateInfo info{};
    info.format = VK_FORMAT_R8G8B8A8_UNORM;
    info.extent = {1920, 1080, 1};
    info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    
    tracker.TrackImage(dummyDevice, dummyImage, &info);
    
    VulkanImageInfo outInfo;
    EXPECT_TRUE(tracker.GetImageInfo(dummyDevice, dummyImage, outInfo));
    EXPECT_EQ(outInfo.format, VK_FORMAT_R8G8B8A8_UNORM);
    EXPECT_EQ(outInfo.extent.width, 1920);
    EXPECT_GT(outInfo.generation, 0);
    
    tracker.UntrackImage(dummyDevice, dummyImage);
    EXPECT_FALSE(tracker.GetImageInfo(dummyDevice, dummyImage, outInfo));
}

TEST_F(VulkanResourceTrackingTest, MemoryMapping) {
    auto& tracker = VulkanResourceTracker::Get();
    
    VkDevice dummyDevice = reinterpret_cast<VkDevice>(0x1000);
    VkDeviceMemory dummyMemory = reinterpret_cast<VkDeviceMemory>(0x3000);
    void* dummyPtr = reinterpret_cast<void*>(0x4000);
    
    tracker.TrackMappedMemory(dummyDevice, dummyMemory, 0, 1024, dummyPtr);
    
    auto mappings = tracker.GetAllMappedMemory();
    EXPECT_EQ(mappings.size(), 1);
    EXPECT_EQ(mappings[0].hostPointer, dummyPtr);
    
    tracker.UntrackMappedMemory(dummyDevice, dummyMemory);
    
    mappings = tracker.GetAllMappedMemory();
    EXPECT_EQ(mappings.size(), 0);
}
