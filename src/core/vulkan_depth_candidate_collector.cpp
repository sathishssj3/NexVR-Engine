#include "vulkan_depth_candidate_collector.h"
#include "vulkan_image_view_tracker.h"
#include "vulkan_image_state_tracker.h"
#include "vulkan_resource_tracker.h"

namespace vrinject {
namespace vulkan {

void VulkanDepthCandidateCollector::CollectCandidates(VkDevice device) {
    if (!device) return;

    auto activePasses = VulkanRenderPassTracker::Get().GetActiveRenderPasses();
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (const auto& pass : activePasses) {
        // Collect explicit depth attachment
        if (pass.depthAttachment) {
            DepthCandidate candidate = CreateCandidateFromAttachment(device, pass.depthAttachment, pass);
            if (candidate.valid) {
                m_frameCandidates.push_back(candidate);
            }
        }
        
        // Sometimes depth is bound as a color attachment (e.g. some G-buffer passes or shadow generation)
        // We evaluate color attachments that might actually be depth formats.
        for (VkImageView view : pass.colorAttachments) {
            if (!view) continue;
            // Only process if it wasn't already the explicit depth attachment
            if (view == pass.depthAttachment) continue;

            DepthCandidate candidate = CreateCandidateFromAttachment(device, view, pass);
            if (candidate.valid) {
                m_frameCandidates.push_back(candidate);
            }
        }
    }
}

std::vector<DepthCandidate> VulkanDepthCandidateCollector::GetAndClearCandidates() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<DepthCandidate> result = std::move(m_frameCandidates);
    m_frameCandidates.clear();
    return result;
}

DepthCandidate VulkanDepthCandidateCollector::CreateCandidateFromAttachment(VkDevice device, VkImageView imageView, const RenderPassRecord& rpRecord) {
    DepthCandidate candidate{};
    candidate.valid = false;

    VulkanImageViewRecord viewRecord;
    if (!VulkanImageViewTracker::Get().GetImageViewRecord(device, imageView, viewRecord)) {
        return candidate;
    }

    VulkanImageInfo imageInfo;
    if (!VulkanResourceTracker::Get().GetImageInfo(device, viewRecord.image, imageInfo)) {
        return candidate;
    }

    // Filter by format: only depth formats
    switch (imageInfo.format) {
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_D32_SFLOAT:
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            break;
        default:
            return candidate; // Not a depth format
    }

    // Identify transient images
    if (imageInfo.usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT) {
        return candidate;
    }

    // Populate candidate
    candidate.identity.nativeHandle = viewRecord.image;
    candidate.identity.epoch.resourceGeneration = imageInfo.generation;
    candidate.identity.width = rpRecord.width > 0 ? rpRecord.width : imageInfo.extent.width;
    candidate.identity.height = rpRecord.height > 0 ? rpRecord.height : imageInfo.extent.height;

    // We can fetch layout state if we want to factor that into ranking
    ImageStateRecord stateRecord;
    if (VulkanImageStateTracker::Get().GetImageState(device, viewRecord.image, stateRecord)) {
        // e.g. check if it's currently in a depth optimal layout
    }

    candidate.valid = true;
    return candidate;
}

} // namespace vulkan
} // namespace vrinject
