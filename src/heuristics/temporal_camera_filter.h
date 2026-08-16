#pragma once
#include "heuristics/camera_candidate.h"
#include <cstdint>

namespace vrinject {

class TemporalCameraFilter {
public:
    static TemporalCameraFilter& Get() {
        static TemporalCameraFilter instance;
        return instance;
    }

    // Pass the best candidate chosen by the CameraRankingEngine for the current frame
    void Update(const CameraCandidate& bestCandidate, uint64_t currentFrame);

    // Get the temporally stable candidate.
    // If we haven't locked onto a stable camera, returns an invalid candidate.
    CameraCandidate GetStableCandidate() const;

    void Reset();

private:
    TemporalCameraFilter() = default;

    enum class FilterState {
        SEARCHING,
        LOCKED,
        COASTING
    };

    FilterState m_state = FilterState::SEARCHING;
    CameraCandidate m_lockedCandidate = {};
    uint64_t m_lockedId = 0;
    uint64_t m_lastSeenFrame = 0;
    
    // Hysteresis counters
    int m_lockCounter = 0;
    const int REQUIRED_LOCK_FRAMES = 3;
    const int ALLOWED_COAST_FRAMES = 10;
};

} // namespace vrinject
