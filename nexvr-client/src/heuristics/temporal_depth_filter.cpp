#include "heuristics/temporal_depth_filter.h"
#include <cmath>

namespace vrinject {

void TemporalDepthFilter::Update(const DepthCandidate& bestFrameCandidate, uint64_t currentFrame) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // If we have no valid candidate this frame
    if (!bestFrameCandidate.valid || bestFrameCandidate.identity.nativeHandle == nullptr) {
        if (m_isLocked) {
            m_releaseCount++;
            m_currentConfidence *= m_config.decayFactor;
            
            if (m_releaseCount >= m_config.releaseFrames) {
                m_isLocked = false;
                m_lockedCandidate = DepthCandidate{};
                m_currentConfidence = 0.0f;
            }
        } else {
            // Not locked and no candidate, reset acquisition
            m_acquisitionCount = 0;
        }
        return;
    }

    // We have a valid candidate
    bool matchesLocked = m_isLocked && 
                         m_lockedCandidate.identity.nativeHandle == bestFrameCandidate.identity.nativeHandle &&
                         m_lockedCandidate.identity.epoch.resourceGeneration == bestFrameCandidate.identity.epoch.resourceGeneration;

    if (matchesLocked) {
        // Reinforce lock
        m_releaseCount = 0;
        m_currentConfidence = 100.0f;
        // Update metadata that might have changed (e.g. resolution if dynamic)
        m_lockedCandidate = bestFrameCandidate;
    } else {
        // Trying to acquire a new lock
        bool matchesAcquiring = m_acquiringIdentity.nativeHandle == bestFrameCandidate.identity.nativeHandle &&
                                m_acquiringIdentity.epoch.resourceGeneration == bestFrameCandidate.identity.epoch.resourceGeneration;

        if (matchesAcquiring) {
            m_acquisitionCount++;
            if (m_acquisitionCount >= m_config.acquireFrames) {
                // We've seen it enough times, lock it!
                m_isLocked = true;
                m_lockedCandidate = bestFrameCandidate;
                m_currentConfidence = 100.0f;
                m_releaseCount = 0;
            }
        } else {
            // New candidate entirely
            m_acquiringIdentity = bestFrameCandidate.identity;
            m_acquisitionCount = 1;
        }

        // If we are currently locked to something else, we let it coast
        if (m_isLocked && !matchesLocked) {
            m_releaseCount++;
            m_currentConfidence *= m_config.decayFactor;
            
            if (m_releaseCount >= m_config.releaseFrames) {
                // Force release
                m_isLocked = false;
                m_lockedCandidate = DepthCandidate{};
                m_currentConfidence = 0.0f;
                
                // If we also just acquired the new one this exact frame, lock it immediately
                if (m_acquisitionCount >= m_config.acquireFrames) {
                    m_isLocked = true;
                    m_lockedCandidate = bestFrameCandidate;
                    m_currentConfidence = 100.0f;
                    m_releaseCount = 0;
                }
            }
        }
    }
}

void TemporalDepthFilter::Reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_isLocked = false;
    m_lockedCandidate = DepthCandidate{};
    m_acquiringIdentity = DepthResourceIdentity{};
    m_acquisitionCount = 0;
    m_releaseCount = 0;
    m_currentConfidence = 0.0f;
}

bool TemporalDepthFilter::GetLockedDepth(DepthCandidate& outLockedCandidate) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_isLocked) {
        outLockedCandidate = m_lockedCandidate;
        outLockedCandidate.confidence = m_currentConfidence;
        return true;
    }
    return false;
}

} // namespace vrinject
