#include "heuristics/depth_delta_tracker.h"
#include <algorithm>

namespace vrinject {

void DepthDeltaTracker::Track(DepthCandidate& candidate, uint64_t currentFrame) {
    if (!candidate.valid) return;

    // 1. Initial heuristic (duration tracking)
    uint64_t lifetime = currentFrame - candidate.identity.epoch.resourceGeneration; // Approximated with frames since creation
    candidate.lifetimeScore = std::min(1.0f, static_cast<float>(lifetime) / 60.0f);
    
    // Temporal stability (how recently was it seen?)
    if (candidate.lastFrameSeen > 0) {
        uint64_t framesSinceSeen = currentFrame - candidate.frameSeen;
        if (framesSinceSeen > 10) {
            candidate.temporalStabilityScore = 0.0f; // Stale
        } else {
            candidate.temporalStabilityScore = 1.0f - (static_cast<float>(framesSinceSeen) / 10.0f);
        }
    } else {
        candidate.temporalStabilityScore = 0.5f; // New candidate
    }

    // Binding frequency (saturates at 10 binds per frame)
    candidate.bindFrequencyScore = std::min(1.0f, static_cast<float>(candidate.bindCount) / 10.0f);
    
    // Clear pattern score (typically cleared exactly once or twice per frame)
    if (candidate.clearCount == 1) {
        candidate.clearPatternScore = 1.0f;
    } else if (candidate.clearCount == 2) {
        candidate.clearPatternScore = 0.8f;
    } else if (candidate.clearCount == 0) {
        candidate.clearPatternScore = 0.2f; // Sometimes depth is not cleared but fully overwritten
    } else {
        candidate.clearPatternScore = 0.5f; // Multiple clears might mean a temporary buffer reused
    }
}

} // namespace vrinject
