#include "camera_delta_tracker.h"
#include <cmath>
#include <algorithm>

namespace vrinject {

static inline float M(const Matrix4x4& mat, int r, int c, bool rowMajor) {
    if (rowMajor) return mat.m[r][c];
    return mat.m[c][r];
}

void CameraDeltaTracker::UpdateDelta(const CameraCandidate& previous, CameraCandidate& current) {
    float delta = CalculateDelta(previous.viewProjection, current.viewProjection, current.rowMajor);
    
    // Accumulate or decay temporal score based on motion
    if (delta > 0.0001f && delta < 0.5f) {
        // Natural camera movement
        current.temporalScore = std::min(previous.temporalScore + 0.1f, 1.0f);
    } else if (delta == 0.0f) {
        // Static camera - could be pause menu or UI
        current.temporalScore = std::max(previous.temporalScore - 0.05f, 0.0f);
    } else {
        // Teleport or cutscene - large delta
        // We preserve some score but penalize heavily to allow relock if it was a cutscene
        current.temporalScore = std::max(previous.temporalScore - 0.2f, 0.1f);
    }
}

float CameraDeltaTracker::CalculateDelta(const Matrix4x4& m1, const Matrix4x4& m2, bool rowMajor) {
    float diff = 0.0f;
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            float v1 = M(m1, r, c, rowMajor);
            float v2 = M(m2, r, c, rowMajor);
            diff += std::abs(v1 - v2);
        }
    }
    return diff;
}

bool CameraDeltaTracker::IsValidCameraMotion(float deltaScore) {
    return deltaScore > 0.1f; // Needs at least some temporal validity
}

} // namespace vrinject
