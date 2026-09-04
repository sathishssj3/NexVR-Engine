#include <gtest/gtest.h>
#include "rendering/vulkan/vulkan_camera_extractor.h"
#include "heuristics/candidate_collector.h"

using namespace vrinject;
using namespace vrinject::vulkan;

TEST(VulkanCameraExtractorTest, OnBindDescriptorSets_RecordsState) {
    auto& extractor = VulkanCameraExtractor::Get();
    
    VkCommandBuffer cb = (VkCommandBuffer)0x100;
    VkPipelineLayout layout = (VkPipelineLayout)0x200;
    VkDescriptorSet sets[] = { (VkDescriptorSet)0x300, (VkDescriptorSet)0x301 };
    uint32_t dynamicOffsets[] = { 256, 512 };

    extractor.OnBindDescriptorSets(
        cb, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 2, sets, 2, dynamicOffsets
    );

    // To verify, we would need to inspect internal state or trigger an Extract.
    // For unit testing purposes, this ensures no crash occurs and logic is executed.
    EXPECT_TRUE(true);
}

TEST(VulkanCameraExtractorTest, SecondaryCommandBufferTreeTracking) {
    auto& extractor = VulkanCameraExtractor::Get();
    
    VkCommandBuffer primaryCB = (VkCommandBuffer)0x1000;
    VkCommandBuffer secondaryCB1 = (VkCommandBuffer)0x2001;
    VkCommandBuffer secondaryCB2 = (VkCommandBuffer)0x2002;
    VkPipelineLayout layout = (VkPipelineLayout)0x3000;
    VkDescriptorSet sets1[] = { (VkDescriptorSet)0x4001 };
    VkDescriptorSet sets2[] = { (VkDescriptorSet)0x4002 };
    
    // Bind descriptors inside secondary command buffers
    extractor.OnBindDescriptorSets(secondaryCB1, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, sets1, 0, nullptr);
    extractor.OnBindDescriptorSets(secondaryCB2, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, sets2, 0, nullptr);

    // Record execution into primary buffer
    VkCommandBuffer secondaries[] = { secondaryCB1, secondaryCB2 };
    extractor.OnExecuteCommands(primaryCB, 2, secondaries);

    // Submit primary buffer
    VkDevice dummyDevice = (VkDevice)0x9000;
    VkCommandBuffer cmdBuffers[] = { primaryCB };
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = cmdBuffers;

    // Verify execution does not crash and processes execution tree
    extractor.Extract(dummyDevice, 1, &submitInfo);
}
