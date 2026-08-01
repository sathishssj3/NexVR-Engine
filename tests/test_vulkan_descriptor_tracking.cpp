#include <gtest/gtest.h>
#include "../src/core/vulkan_descriptor_tracker.h"

using namespace vrinject;
using namespace vrinject::vulkan;

class VulkanDescriptorTrackingTest : public ::testing::Test {
protected:
    void SetUp() override {
    }
};

TEST_F(VulkanDescriptorTrackingTest, DescriptorSetUpdates) {
    auto& tracker = VulkanDescriptorTracker::Get();
    
    VkDevice dummyDevice = reinterpret_cast<VkDevice>(0x1000);
    VkDescriptorPool dummyPool = reinterpret_cast<VkDescriptorPool>(0x2000);
    VkDescriptorSet dummySet = reinterpret_cast<VkDescriptorSet>(0x3000);
    
    // Allocate
    tracker.TrackDescriptorSets(dummyDevice, dummyPool, 1, &dummySet);
    
    // Update
    VkWriteDescriptorSet write{};
    write.dstSet = dummySet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = reinterpret_cast<VkBuffer>(0x4000);
    write.pBufferInfo = &bufferInfo;
    
    tracker.UpdateDescriptorSets(dummyDevice, 1, &write);
    
    // Verify
    VulkanDescriptorSetInfo info;
    EXPECT_TRUE(tracker.GetDescriptorSetInfo(dummyDevice, dummySet, info));
    EXPECT_EQ(info.bindings.size(), 1);
    EXPECT_EQ(info.bindings[0].buffer, bufferInfo.buffer);
    
    // Free
    tracker.UntrackDescriptorSets(dummyDevice, dummyPool, 1, &dummySet);
    EXPECT_FALSE(tracker.GetDescriptorSetInfo(dummyDevice, dummySet, info));
}

TEST_F(VulkanDescriptorTrackingTest, PoolReset) {
    auto& tracker = VulkanDescriptorTracker::Get();
    
    VkDevice dummyDevice = reinterpret_cast<VkDevice>(0x1000);
    VkDescriptorPool dummyPool = reinterpret_cast<VkDescriptorPool>(0x2000);
    VkDescriptorSet dummySet1 = reinterpret_cast<VkDescriptorSet>(0x3001);
    VkDescriptorSet dummySet2 = reinterpret_cast<VkDescriptorSet>(0x3002);
    
    VkDescriptorSet sets[] = {dummySet1, dummySet2};
    tracker.TrackDescriptorSets(dummyDevice, dummyPool, 2, sets);
    
    VulkanDescriptorSetInfo info;
    EXPECT_TRUE(tracker.GetDescriptorSetInfo(dummyDevice, dummySet1, info));
    
    tracker.ResetDescriptorPool(dummyDevice, dummyPool);
    
    EXPECT_FALSE(tracker.GetDescriptorSetInfo(dummyDevice, dummySet1, info));
    EXPECT_FALSE(tracker.GetDescriptorSetInfo(dummyDevice, dummySet2, info));
}

TEST_F(VulkanDescriptorTrackingTest, ABAResourceReallocationSafety) {
    auto& tracker = VulkanDescriptorTracker::Get();
    
    VkDevice dummyDevice = reinterpret_cast<VkDevice>(0x1000);
    VkDescriptorPool pool1 = reinterpret_cast<VkDescriptorPool>(0x2000);
    
    // Allocate handle A
    VkDescriptorSet handleA = reinterpret_cast<VkDescriptorSet>(0x7000);
    tracker.TrackDescriptorSets(dummyDevice, pool1, 1, &handleA);
    
    VkWriteDescriptorSet write1{};
    write1.dstSet = handleA;
    write1.dstBinding = 0;
    write1.descriptorCount = 1;
    write1.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    VkDescriptorBufferInfo bufInfo1{ reinterpret_cast<VkBuffer>(0x8001), 0, 64 };
    write1.pBufferInfo = &bufInfo1;
    tracker.UpdateDescriptorSets(dummyDevice, 1, &write1);

    // Free handle A
    tracker.UntrackDescriptorSets(dummyDevice, pool1, 1, &handleA);
    
    // Immediately reallocate the SAME memory address handleA under Pool 2
    VkDescriptorPool pool2 = reinterpret_cast<VkDescriptorPool>(0x2001);
    tracker.TrackDescriptorSets(dummyDevice, pool2, 1, &handleA);

    // Query before write must NOT return old binding data from stale generation
    VulkanDescriptorSetInfo info;
    EXPECT_TRUE(tracker.GetDescriptorSetInfo(dummyDevice, handleA, info));
    EXPECT_TRUE(info.bindings.empty() || info.bindings[0].buffer != bufInfo1.buffer) 
        << "Stale binding data leaked across ABA handle reallocation!";
}
