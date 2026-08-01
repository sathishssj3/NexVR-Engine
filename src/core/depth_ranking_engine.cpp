#include "depth_ranking_engine.h"
#include <algorithm>

namespace vrinject {

void DepthRankingEngine::Rank(std::vector<DepthCandidate>& candidates, const DepthRankingWeights& weights) {
    for (auto& candidate : candidates) {
        if (!candidate.valid) {
            candidate.totalScore = 0.0f;
            continue;
        }

        candidate.totalScore = 
            (candidate.lifetimeScore * weights.lifetime) +
            (candidate.resolutionScore * weights.resolutionMatch) +
            (candidate.bindFrequencyScore * weights.bindingFrequency) +
            (candidate.clearPatternScore * weights.clearPattern) +
            (candidate.temporalStabilityScore * weights.temporalStability) +
            (candidate.classifierScore * weights.classifier);
            
        candidate.confidence = candidate.totalScore * 100.0f;
    }

    // Sort descending by score
    std::sort(candidates.begin(), candidates.end(), [](const DepthCandidate& a, const DepthCandidate& b) {
        return a.totalScore > b.totalScore;
    });
}

} // namespace vrinject
