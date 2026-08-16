#pragma once
#include "heuristics/camera_snapshot.h"
#include "heuristics/camera_candidate.h"

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

    // Supplies the frame's colour resource (normally RenderFrameSnapshot::BackBufferIdentity()).
    // A CameraCandidate describes a constant buffer of matrices and carries no texture, but
    // CameraSnapshot::IsValid() requires a resource identity because the stereo pass samples
    // that texture. Call this each frame before Update(), mirroring how DepthLockManager
    // receives its resource context from FrameCoordinator. Without it every snapshot is
    // invalid and stereo can never engage.
    void SetResourceContext(const GraphicsResourceIdentity& identity);

    CameraSnapshot GetSnapshot() const;
    CameraLockState GetState() const { return m_state; }

    void Reset();

private:
    CameraLockManager() = default;

    CameraLockState m_state = CameraLockState::UNLOCKED;
    CameraSnapshot m_snapshot = {};
    GraphicsResourceIdentity m_resourceContext = {};
    uint64_t m_lockedId = 0;
    uint64_t m_lastSeenFrame = 0;
    float m_lockedConfidence = 0.0f;
};

} // namespace vrinject
