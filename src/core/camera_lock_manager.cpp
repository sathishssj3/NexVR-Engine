#include "camera_lock_manager.h"
#include <cstring>

namespace vrinject {

void CameraLockManager::Reset() {
    m_state = CameraLockState::UNLOCKED;
    std::memset(&m_snapshot, 0, sizeof(CameraSnapshot));
    // Reset() runs on device loss, which invalidates the backbuffer handle. Drop the
    // cached identity so a dangling ID3D12Resource*/VkImage from the previous epoch can
    // never be stamped into a snapshot; FrameCoordinator re-supplies it next frame.
    m_resourceContext = GraphicsResourceIdentity{};
    m_lockedId = 0;
    m_lastSeenFrame = 0;
    m_lockedConfidence = 0.0f;
}

void CameraLockManager::Update(const CameraCandidate& bestCandidate, uint64_t currentFrame) {
    bool hasCandidate = bestCandidate.valid && bestCandidate.id != 0;

    switch (m_state) {
        case CameraLockState::UNLOCKED:
            if (hasCandidate) {
                m_state = CameraLockState::SEARCHING;
            }
            break;
            
        case CameraLockState::SEARCHING:
            if (hasCandidate) {
                m_state = CameraLockState::CANDIDATE_FOUND;
                m_lockedId = bestCandidate.id;
            } else {
                m_state = CameraLockState::UNLOCKED;
            }
            break;

        case CameraLockState::CANDIDATE_FOUND:
            if (hasCandidate && bestCandidate.id == m_lockedId) {
                m_state = CameraLockState::VERIFYING;
            } else {
                m_state = CameraLockState::SEARCHING;
            }
            break;

        case CameraLockState::VERIFYING:
            if (hasCandidate && bestCandidate.id == m_lockedId && bestCandidate.confidence >= 0.5f) {
                m_state = CameraLockState::LOCKED;
            } else {
                m_state = CameraLockState::SEARCHING;
            }
            break;

        case CameraLockState::LOCKED:
            if (hasCandidate && bestCandidate.id == m_lockedId) {
                if (bestCandidate.confidence < 0.5f) {
                    m_state = CameraLockState::REVERIFYING;
                }
            } else {
                m_state = CameraLockState::LOST;
            }
            break;

        case CameraLockState::REVERIFYING:
            if (hasCandidate && bestCandidate.id == m_lockedId) {
                if (bestCandidate.confidence >= 0.5f) {
                    m_state = CameraLockState::LOCKED;
                } else if (bestCandidate.confidence < 0.3f) {
                    m_state = CameraLockState::RELOCKING;
                }
            } else {
                m_state = CameraLockState::LOST;
            }
            break;

        case CameraLockState::LOST:
            if (hasCandidate && bestCandidate.id == m_lockedId) {
                m_state = CameraLockState::RELOCKING;
            } else {
                // If it's been lost for a while, transition to SEARCHING
                if (currentFrame - m_lastSeenFrame > 10) {
                    m_state = CameraLockState::SEARCHING;
                }
            }
            break;

        case CameraLockState::RELOCKING:
            if (hasCandidate && bestCandidate.id == m_lockedId && bestCandidate.confidence >= 0.5f) {
                m_state = CameraLockState::LOCKED;
            } else if (currentFrame - m_lastSeenFrame > 60) {
                m_state = CameraLockState::SEARCHING;
            }
            break;

        case CameraLockState::FAILED:
            // Terminal state or requires explicit reset
            break;
    }

    if (hasCandidate && bestCandidate.id == m_lockedId) {
        m_lastSeenFrame = currentFrame;
        m_lockedConfidence = bestCandidate.confidence;

        // Update snapshot
        m_snapshot.frame = currentFrame;
        m_snapshot.vp = bestCandidate.viewProjection;
        m_snapshot.projection = bestCandidate.projection;
        m_snapshot.view = bestCandidate.view;
        m_snapshot.confidence = bestCandidate.confidence;
        m_snapshot.reversedZ = bestCandidate.reversedZ;
        // CameraSnapshot::IsValid() requires a resource identity, and a CameraCandidate
        // has none (it describes a constant buffer, not a texture). Carry the frame's
        // colour resource supplied by SetResourceContext(); without this the snapshot is
        // permanently invalid, CompatibilityScorer clears shouldAttemptStereo, and stereo
        // never engages in any game.
        m_snapshot.resourceIdentity = m_resourceContext;
        m_snapshot.valid = true;
    } else if (m_state == CameraLockState::SEARCHING || m_state == CameraLockState::UNLOCKED) {
        m_snapshot.valid = false;
        m_snapshot.confidence = 0.0f;
    }
}

void CameraLockManager::SetResourceContext(const GraphicsResourceIdentity& identity) {
    m_resourceContext = identity;
}

CameraSnapshot CameraLockManager::GetSnapshot() const {
    return m_snapshot;
}

} // namespace vrinject
