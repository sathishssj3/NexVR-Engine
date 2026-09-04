#pragma once
#include "heuristics/camera_candidate.h"
#include <vector>

namespace vrinject {

class CameraRankingEngine {
public:
    // Applies scoring weights and returns the best candidate ID
    // 35% Motion, 20% Projection, 20% Update Freq, 15% Lifetime, 10% Classifier
    static bool RankCandidates(const std::vector<CameraCandidate>& candidates, uint64_t currentFrame, CameraCandidate& outBest);
};

} // namespace vrinject
