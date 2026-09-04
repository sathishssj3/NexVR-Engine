#pragma once

#include <cstdint>
#include <mutex>
#include "heuristics/depth_candidate.h"

namespace vrinject {

struct TemporalDepthFilterConfig {
    uint32_t acquireFrames = 4;
    uint32_t releaseFrames = 12;
    float decayFactor = 0.85f;
};

class TemporalDepthFilter {
public:
    TemporalDepthFilter() = default;

    // Pass the best candidate chosen by the DepthRankingEngine for the current frame
    void Update(const DepthCandidate& bestFrameCandidate, uint64_t currentFrame);

    // Reset the filter state (e.g. on swapchain recreation)
    void Reset();

    // Get the currently locked depth candidate (if any) and its confidence level
    bool GetLockedDepth(DepthCandidate& outLockedCandidate) const;

private:

    mutable std::mutex m_mutex;
    TemporalDepthFilterConfig m_config;

    DepthCandidate m_lockedCandidate{};
    bool m_isLocked = false;
    
    // Tracking current acquisition
    DepthResourceIdentity m_acquiringIdentity{};
    uint32_t m_acquisitionCount = 0;
    
    // Tracking current release
    uint32_t m_releaseCount = 0;
    
    float m_currentConfidence = 0.0f;
};

} // namespace vrinject
