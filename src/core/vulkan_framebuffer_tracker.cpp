#include "vulkan_framebuffer_tracker.h"

namespace vrinject {
namespace vulkan {

void VulkanFramebufferTracker::OnCreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, VkFramebuffer framebuffer) {
    if (!device || !pCreateInfo || !framebuffer) return;

    VulkanFramebufferRecord record;
    record.framebuffer = framebuffer;
    record.device = device;
    record.renderPass = pCreateInfo->renderPass;
    record.width = pCreateInfo->width;
    record.height = pCreateInfo->height;
    record.layers = pCreateInfo->layers;
    
    if (pCreateInfo->pAttachments && pCreateInfo->attachmentCount > 0) {
        record.attachments.assign(pCreateInfo->pAttachments, pCreateInfo->pAttachments + pCreateInfo->attachmentCount);
    }

    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_framebuffers[{device, framebuffer}] = record;
}

void VulkanFramebufferTracker::OnDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer) {
    if (!device || !framebuffer) return;

    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_framebuffers.erase({device, framebuffer});
}

bool VulkanFramebufferTracker::GetFramebufferRecord(VkDevice device, VkFramebuffer framebuffer, VulkanFramebufferRecord& outRecord) const {
    if (!device || !framebuffer) return false;

    std::shared_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_framebuffers.find({device, framebuffer});
    if (it != m_framebuffers.end()) {
        outRecord = it->second;
        return true;
    }
    return false;
}

} // namespace vulkan
} // namespace vrinject
