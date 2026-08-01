#include "vulkan_render_pass_tracker.h"
#include "vulkan_framebuffer_tracker.h"

namespace vrinject {
namespace vulkan {

void VulkanRenderPassTracker::OnCmdBeginRenderPass(VkDevice device, VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin) {
    if (!device || !commandBuffer || !pRenderPassBegin) return;

    RenderPassRecord record;
    record.commandBuffer = commandBuffer;
    record.device = device;
    record.renderPass = pRenderPassBegin->renderPass;
    record.framebuffer = pRenderPassBegin->framebuffer;
    record.width = pRenderPassBegin->renderArea.extent.width;
    record.height = pRenderPassBegin->renderArea.extent.height;
    record.dynamicRendering = false;

    // Resolve attachments from the Framebuffer
    VulkanFramebufferRecord fbRecord;
    if (VulkanFramebufferTracker::Get().GetFramebufferRecord(device, pRenderPassBegin->framebuffer, fbRecord)) {
        // Typically, we would need the VkRenderPass description to know WHICH attachment is depth vs color.
        // For observation, we can store all attachments.
        // But a robust way is to query the render pass. Since we can't easily parse an opaque VkRenderPass
        // without tracking vkCreateRenderPass (which we aren't explicitly told to do), we might defer
        // exact depth vs color identification to the candidate collector which can check the image format.
        
        // For now, we will store them all in colorAttachments, and the candidate collector will sort them out
        // by looking up the image format.
        record.colorAttachments = fbRecord.attachments;
        record.layers = fbRecord.layers;
        if (record.width == 0) record.width = fbRecord.width;
        if (record.height == 0) record.height = fbRecord.height;
    }

    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_activePasses[commandBuffer] = record;
}

void VulkanRenderPassTracker::OnCmdEndRenderPass(VkDevice device, VkCommandBuffer commandBuffer) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_activePasses.erase(commandBuffer);
}

void VulkanRenderPassTracker::OnCmdBeginRendering(VkDevice device, VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo) {
    if (!device || !commandBuffer || !pRenderingInfo) return;

    RenderPassRecord record;
    record.commandBuffer = commandBuffer;
    record.device = device;
    record.dynamicRendering = true;
    record.width = pRenderingInfo->renderArea.extent.width;
    record.height = pRenderingInfo->renderArea.extent.height;
    record.layers = pRenderingInfo->layerCount;

    if (pRenderingInfo->pDepthAttachment && pRenderingInfo->pDepthAttachment->imageView) {
        record.depthAttachment = pRenderingInfo->pDepthAttachment->imageView;
    }
    
    for (uint32_t i = 0; i < pRenderingInfo->colorAttachmentCount; ++i) {
        if (pRenderingInfo->pColorAttachments[i].imageView) {
            record.colorAttachments.push_back(pRenderingInfo->pColorAttachments[i].imageView);
        }
    }

    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_activePasses[commandBuffer] = record;
}

void VulkanRenderPassTracker::OnCmdEndRendering(VkDevice device, VkCommandBuffer commandBuffer) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_activePasses.erase(commandBuffer);
}

std::vector<RenderPassRecord> VulkanRenderPassTracker::GetActiveRenderPasses() const {
    std::vector<RenderPassRecord> passes;
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    passes.reserve(m_activePasses.size());
    for (const auto& pair : m_activePasses) {
        passes.push_back(pair.second);
    }
    return passes;
}

} // namespace vulkan
} // namespace vrinject
