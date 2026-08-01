#include "vulkan_image_view_tracker.h"

namespace vrinject {
namespace vulkan {

void VulkanImageViewTracker::OnCreateImageView(VkDevice device, const VkImageViewCreateInfo* pCreateInfo, VkImageView imageView) {
    if (!device || !pCreateInfo || !imageView) return;

    VulkanImageViewRecord record;
    record.imageView = imageView;
    record.device = device;
    record.image = pCreateInfo->image;
    record.format = pCreateInfo->format;
    record.subresourceRange = pCreateInfo->subresourceRange;

    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_imageViews[{device, imageView}] = record;
}

void VulkanImageViewTracker::OnDestroyImageView(VkDevice device, VkImageView imageView) {
    if (!device || !imageView) return;

    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_imageViews.erase({device, imageView});
}

bool VulkanImageViewTracker::GetImageViewRecord(VkDevice device, VkImageView imageView, VulkanImageViewRecord& outRecord) const {
    if (!device || !imageView) return false;

    std::shared_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_imageViews.find({device, imageView});
    if (it != m_imageViews.end()) {
        outRecord = it->second;
        return true;
    }
    return false;
}

} // namespace vulkan
} // namespace vrinject
