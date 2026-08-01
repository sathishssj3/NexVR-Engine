#include "temporal_camera_filter.h"
#include <cstring>

namespace vrinject {

void TemporalCameraFilter::Reset() {
    m_state = FilterState::SEARCHING;
    std::memset(&m_lockedCandidate, 0, sizeof(CameraCandidate));
    m_lockedId = 0;
    m_lastSeenFrame = 0;
    m_lockCounter = 0;
}

void TemporalCameraFilter::Update(const CameraCandidate& bestCandidate, uint64_t currentFrame) {
    bool hasCandidate = bestCandidate.valid && bestCandidate.id != 0;

    switch (m_state) {
        case FilterState::SEARCHING:
            if (hasCandidate) {
                if (m_lockedId == bestCandidate.id) {
                    m_lockCounter++;
                } else {
                    m_lockedId = bestCandidate.id;
                    m_lockCounter = 1;
                }
                
                if (m_lockCounter >= REQUIRED_LOCK_FRAMES) {
                    m_state = FilterState::LOCKED;
                    m_lockedCandidate = bestCandidate;
                    m_lastSeenFrame = currentFrame;
                }
            } else {
                m_lockCounter = 0;
                m_lockedId = 0;
            }
            break;

        case FilterState::LOCKED:
            if (hasCandidate && bestCandidate.id == m_lockedId) {
                m_lockedCandidate = bestCandidate;
                m_lastSeenFrame = currentFrame;
            } else {
                m_state = FilterState::COASTING;
            }
            break;

        case FilterState::COASTING:
            if (hasCandidate && bestCandidate.id == m_lockedId) {
                m_state = FilterState::LOCKED;
                m_lockedCandidate = bestCandidate;
                m_lastSeenFrame = currentFrame;
            } else {
                if (currentFrame - m_lastSeenFrame > ALLOWED_COAST_FRAMES) {
                    // Lost it for too long, go back to searching
                    m_state = FilterState::SEARCHING;
                    m_lockCounter = 0;
                    if (hasCandidate) {
                        m_lockedId = bestCandidate.id;
                        m_lockCounter = 1;
                    } else {
                        m_lockedId = 0;
                    }
                    std::memset(&m_lockedCandidate, 0, sizeof(CameraCandidate));
                }
            }
            break;
    }
}

CameraCandidate TemporalCameraFilter::GetStableCandidate() const {
    if (m_state == FilterState::LOCKED || m_state == FilterState::COASTING) {
        return m_lockedCandidate;
    }
    CameraCandidate invalid = {};
    return invalid;
}

} // namespace vrinject
