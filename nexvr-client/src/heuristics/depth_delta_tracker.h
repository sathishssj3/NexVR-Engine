#pragma once
#include "heuristics/depth_candidate.h"
#include <cstdint>

namespace vrinject {

class DepthDeltaTracker {
public:
    // Analyzes frequencies over time to produce temporal and pattern scores
    static void Track(DepthCandidate& candidate, uint64_t currentFrame);
};

} // namespace vrinject
