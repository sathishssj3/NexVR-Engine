#pragma once
#include "heuristics/camera_candidate.h"
#include <vector>

namespace vrinject {

class CameraValidator {
public:
    // Final verification layer
    // Rejects UI cameras, reflection cameras, static cameras, etc.
    static bool ValidateCandidate(const CameraCandidate& candidate);
};

} // namespace vrinject
