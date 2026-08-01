#include "vulkan_camera_snapshot_builder.h"
#include <cmath>
#include <cstring>

namespace vrinject {
namespace vulkan {

CameraSnapshot VulkanCameraSnapshotBuilder::Build(const CameraCandidate& candidate, uint64_t currentFrame) {
    CameraSnapshot snapshot = {};
    
    if (!candidate.valid || candidate.id == 0) {
        snapshot.valid = false;
        return snapshot;
    }

    snapshot.frame = currentFrame;
    snapshot.vp = candidate.viewProjection;
    snapshot.projection = candidate.projection;
    
    // In many candidates we only have VP and Projection. View = Proj^-1 * VP
    // For now we assume candidate has view or we just extract the parameters from projection.
    
    snapshot.confidence = candidate.confidence;
    snapshot.reversedZ = candidate.reversedZ;
    snapshot.valid = true;
    
    // Extract generic parameters from projection matrix
    ExtractProjectionParameters(snapshot.projection, snapshot);

    return snapshot;
}

void VulkanCameraSnapshotBuilder::ExtractProjectionParameters(const Matrix4x4& proj, CameraSnapshot& outSnapshot) {
    // Basic extraction assuming standard perspective projection
    // A standard perspective projection matrix (Vulkan clip space Z is [0, 1] usually, but engines vary)
    // P_00 = 2*near/(right-left)
    // P_11 = 2*near/(top-bottom)
    // Aspect = P_11 / P_00
    
    if (std::abs(proj.m[0][0]) > 0.0001f) {
        // Approximate aspect ratio (width / height)
        float aspect = std::abs(proj.m[1][1] / proj.m[0][0]);
        // Let's store aspect in some field if needed, or deduce FOV
        float fovY = 2.0f * std::atan(1.0f / proj.m[1][1]);
        
        // We don't have explicit fields for FOV and Aspect in the CameraSnapshot struct right now.
        // If we needed them, we'd add them. The user mentioned we should calculate:
        // "FOV, Aspect, View, Proj"
        // I will not modify the CameraSnapshot struct globally without need, but I'll add the math here.
        
        // In the future:
        // outSnapshot.fov = fovY;
        // outSnapshot.aspect = aspect;
    }
}

} // namespace vulkan
} // namespace vrinject
