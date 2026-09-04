#include "heuristics/camera_validator.h"

namespace vrinject {

bool CameraValidator::ValidateCandidate(const CameraCandidate& candidate) {
    if (!candidate.valid) return false;

    // Reject low confidence (classifier didn't like it)
    if (candidate.confidence < 0.4f) return false;

    // Reject static cameras unless they are extremely fresh (could be moving next frame)
    if (candidate.temporalScore < 0.1f && candidate.updateCount > 10) return false;

    // Future improvements:
    // - Reject based on known shadow map resolutions (if we tracked viewport)
    // - Reject minimap orthogonal matrices (classifier should catch this, but just in case)
    // - Reject UI matrices (usually orthographic)
    
    return true;
}

} // namespace vrinject
