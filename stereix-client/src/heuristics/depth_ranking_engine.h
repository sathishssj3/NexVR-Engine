#pragma once
#include "heuristics/depth_candidate.h"
#include <vector>

namespace vrinject {

struct DepthRankingWeights {
    float lifetime = 0.25f;
    float resolutionMatch = 0.20f;
    float bindingFrequency = 0.20f;
    float clearPattern = 0.15f;
    float temporalStability = 0.15f;
    float classifier = 0.05f;
};

class DepthRankingEngine {
public:
    static void Rank(std::vector<DepthCandidate>& candidates, const DepthRankingWeights& weights = DepthRankingWeights{});
};

} // namespace vrinject
