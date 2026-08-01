#pragma once
#include "camera_candidate.h"

namespace vrinject {

class CameraDeltaTracker {
public:
    CameraDeltaTracker() = default;

    // Evaluates the movement of a candidate since the last frame and updates its temporalScore.
    // previous is the state of the candidate from the previous frame.
    // current is the candidate from the current frame.
    static void UpdateDelta(const CameraCandidate& previous, CameraCandidate& current);

    // Determines if the movement represents typical camera motion.
    static bool IsValidCameraMotion(float deltaScore);
    
private:
    static float CalculateDelta(const Matrix4x4& m1, const Matrix4x4& m2, bool rowMajor);
};

} // namespace vrinject
