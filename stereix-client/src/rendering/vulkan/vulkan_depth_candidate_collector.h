#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include "heuristics/depth_candidate.h"
#include "rendering/vulkan/vulkan_render_pass_tracker.h"

namespace vrinject {
namespace vulkan {

class VulkanDepthCandidateCollector {
public:
    static VulkanDepthCandidateCollector& Get() {
        static VulkanDepthCandidateCollector instance;
        return instance;
    }

    // Called during vkQueueSubmit or vkQueuePresentKHR to collect depth candidates
    // from the tracked render passes
    void CollectCandidates(VkDevice device);

    // Get and clear collected candidates for the frame
    std::vector<DepthCandidate> GetAndClearCandidates();

private:
    VulkanDepthCandidateCollector() = default;

    DepthCandidate CreateCandidateFromAttachment(VkDevice device, VkImageView imageView, const RenderPassRecord& rpRecord);

    mutable std::mutex m_mutex;
    std::vector<DepthCandidate> m_frameCandidates;
};

} // namespace vulkan
} // namespace vrinject
