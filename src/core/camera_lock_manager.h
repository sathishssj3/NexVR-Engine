#pragma once
#include "camera_snapshot.h"
#include "camera_candidate.h"

namespace vrinject {

enum class CameraLockState {
    UNLOCKED,
    SEARCHING,
    CANDIDATE_FOUND,
    VERIFYING,
    LOCKED,
    REVERIFYING,
    LOST,
    RELOCKING,
    FAILED
};

class CameraLockManager {
public:
    static CameraLockManager& Get() {
        static CameraLockManager instance;
        return instance;
    }

    void Update(const CameraCandidate& bestCandidate, uint64_t currentFrame);
    
    CameraSnapshot GetSnapshot() const;
    CameraLockState GetState() const { return m_state; }

    void Reset();

private:
    CameraLockManager() = default;

    CameraLockState m_state = CameraLockState::UNLOCKED;
    CameraSnapshot m_snapshot = {};
    uint64_t m_lockedId = 0;
    uint64_t m_lastSeenFrame = 0;
    float m_lockedConfidence = 0.0f;
};

} // namespace vrinject
