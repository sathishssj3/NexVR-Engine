#pragma once

#include <vulkan/vulkan.h>
#include "../core/depth_snapshot.h"
#include "../core/depth_candidate.h"

namespace vrinject {
namespace vulkan {

struct VulkanDepthSnapshot : public DepthSnapshot {
    VkImage image = nullptr;
    VkImageView imageView = nullptr;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageLayout imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t sampleCount = 1;
};

class VulkanDepthSnapshotBuilder {
public:
    static VulkanDepthSnapshotBuilder& Get() {
        static VulkanDepthSnapshotBuilder instance;
        return instance;
    }

    VulkanDepthSnapshot BuildSnapshot(VkDevice device, const DepthCandidate& lockedCandidate) const;

private:
    VulkanDepthSnapshotBuilder() = default;
};

} // namespace vulkan
} // namespace vrinject
