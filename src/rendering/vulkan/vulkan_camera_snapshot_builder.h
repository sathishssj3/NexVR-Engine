#pragma once
#include "heuristics/camera_snapshot.h"
#include "heuristics/camera_candidate.h"

namespace vrinject {
namespace vulkan {

class VulkanCameraSnapshotBuilder {
public:
    static CameraSnapshot Build(const CameraCandidate& candidate, uint64_t currentFrame);

private:
    static void ExtractProjectionParameters(const Matrix4x4& proj, CameraSnapshot& outSnapshot);
};

} // namespace vulkan
} // namespace vrinject
