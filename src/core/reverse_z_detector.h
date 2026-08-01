#pragma once
#include "depth_candidate.h"
#include "depth_convention.h"

namespace vrinject {

class ReverseZDetector {
public:
    // Uses clear values or projection matrix heuristics (if available) to detect Reverse-Z.
    // For this milestone, we use a simple heuristic based on the clear value.
    // E.g., ClearDepthStencilView with depth=0.0f is a strong indicator of Reverse-Z.
    static DepthConvention Detect(const DepthCandidate& candidate, float lastClearDepth);
};

} // namespace vrinject
