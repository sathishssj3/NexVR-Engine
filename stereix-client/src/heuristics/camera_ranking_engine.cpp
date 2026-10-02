#include "heuristics/camera_ranking_engine.h"
#include <algorithm>

namespace vrinject {

bool CameraRankingEngine::RankCandidates(const std::vector<CameraCandidate>& candidates, uint64_t currentFrame, CameraCandidate& outBest) {
    if (candidates.empty()) return false;

    int bestIdx = -1;
    float bestScore = -1.0f;

    for (size_t i = 0; i < candidates.size(); ++i) {
        const auto& c = candidates[i];
        if (!c.valid) continue;

        // Weights:
        // Motion: 0.35
        // Projection: 0.20 (Is it reverse-Z? Often standard in modern engines, but let's give 0.2 for valid)
        // Update Freq: 0.20
        // Lifetime: 0.15
        // Classifier: 0.10

        float motionScore = c.temporalScore * 0.35f;
        float projScore = 0.20f; // Since it's valid
        
        float freq = c.updateCount > 1 ? 1.0f : 0.5f; // rough approx
        float freqScore = freq * 0.20f;
        
        float lifetime = std::min(1.0f, static_cast<float>(currentFrame - c.firstSeenFrame) / 60.0f);
        float lifeScore = lifetime * 0.15f;

        float classScore = c.confidence * 0.10f;

        float totalScore = motionScore + projScore + freqScore + lifeScore + classScore;

        if (totalScore > bestScore) {
            bestScore = totalScore;
            bestIdx = static_cast<int>(i);
        }
    }

    if (bestIdx != -1) {
        outBest = candidates[bestIdx];
        outBest.confidence = bestScore; // Overwrite confidence with the final score for lock manager
        return true;
    }

    return false;
}

} // namespace vrinject
