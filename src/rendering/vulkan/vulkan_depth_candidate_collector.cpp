#include "rendering/vulkan/vulkan_depth_candidate_collector.h"
#include "rendering/vulkan/vulkan_image_view_tracker.h"
#include "rendering/vulkan/vulkan_image_state_tracker.h"
#include "rendering/vulkan/vulkan_resource_tracker.h"

namespace vrinject {
namespace vulkan {

void VulkanDepthCandidateCollector::CollectCandidates(VkDevice device) {
    if (!device) return;

    auto allViews = VulkanImageViewTracker::Get().GetAllImageViews();
    
    std::lock_guard<std::mutex> lock(m_mutex);
    m_frameCandidates.clear();
    
    for (const auto& viewRecord : allViews) {
        // Basic check if it's a depth format
        bool isDepth = false;
        switch (viewRecord.format) {
            case VK_FORMAT_D16_UNORM:
            case VK_FORMAT_X8_D24_UNORM_PACK32:
            case VK_FORMAT_D32_SFLOAT:
            case VK_FORMAT_D16_UNORM_S8_UINT:
            case VK_FORMAT_D24_UNORM_S8_UINT:
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
                isDepth = true;
                break;
            default:
                break;
        }
        
        if (!isDepth) continue;

        VulkanImageInfo imageInfo;
        if (!VulkanResourceTracker::Get().GetImageInfo(device, viewRecord.image, imageInfo)) {
            continue;
        }

        DepthCandidate candidate{};
        candidate.valid = true;
        candidate.identity.nativeHandle = (void*)viewRecord.imageView;
        candidate.identity.backend = GraphicsBackend::Vulkan;
        candidate.identity.width = imageInfo.extent.width;
        candidate.identity.height = imageInfo.extent.height;
        candidate.bindCount = 1; // Fake bind count so it scores normally
        candidate.clearCount = 0;
        
        m_frameCandidates.push_back(candidate);
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
