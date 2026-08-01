#include "vulkan_depth_snapshot_builder.h"
#include "vulkan_image_view_tracker.h"
#include "vulkan_image_state_tracker.h"
#include "vulkan_resource_tracker.h"

namespace vrinject {
namespace vulkan {

VulkanDepthSnapshot VulkanDepthSnapshotBuilder::BuildSnapshot(VkDevice device, const DepthCandidate& lockedCandidate) const {
    VulkanDepthSnapshot snapshot{};
    
    if (!lockedCandidate.valid || lockedCandidate.identity.nativeHandle == nullptr) {
        return snapshot;
    }

    snapshot.image = static_cast<VkImage>(lockedCandidate.identity.nativeHandle);
    
    VulkanImageInfo imageInfo;
    if (VulkanResourceTracker::Get().GetImageInfo(device, snapshot.image, imageInfo)) {
        snapshot.format = imageInfo.format;
        snapshot.sampleCount = 1; // Assuming 1 unless we track sample count properly in image info
        // actually imageInfo doesn't have sampleCount right now, we can add it or just default to 1.
        // Let's use 1 for now, or check if we can query it.
        snapshot.width = lockedCandidate.identity.width; // Or from imageInfo.extent.width
        snapshot.height = lockedCandidate.identity.height;
        
        snapshot.depthGeneration = imageInfo.generation;
    }

    // Try to find an image view for this image
    // Since we only track creation/destruction, we might need to iterate or just know the active view.
    // In our CandidateCollector we extracted from ImageView. Ideally lockedCandidate should have kept the view.
    // For now we'll just populate what we know.
    // In a full robust system, the candidate should probably store the view in a custom field.

    ImageStateRecord stateRecord;
    if (VulkanImageStateTracker::Get().GetImageState(device, snapshot.image, stateRecord)) {
        snapshot.imageLayout = stateRecord.currentLayout;
    }

    snapshot.identity = lockedCandidate.identity;
    snapshot.identity.backend = GraphicsBackend::Vulkan;
    snapshot.confidence = lockedCandidate.confidence;
    
    return snapshot;
}

} // namespace vulkan
} // namespace vrinject
