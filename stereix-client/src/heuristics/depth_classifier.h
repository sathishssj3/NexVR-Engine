#pragma once
#include "heuristics/depth_candidate.h"

namespace vrinject {

class DepthClassifier {
public:
    // Pure function. No state. Returns a baseline score based on format.
    // Rejects invalid formats by setting valid = false and returning 0.
    static void Classify(DepthCandidate& candidate);
};

} // namespace vrinject
