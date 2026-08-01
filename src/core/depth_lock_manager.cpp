#include "depth_lock_manager.h"
#include "depth_classifier.h"
#include "depth_delta_tracker.h"
#include "depth_validator.h"
#include "depth_ranking_engine.h"
#include "reverse_z_detector.h"

namespace vrinject {

void DepthLockManager::OnDeviceLost() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state = DepthLockState::LOST;
    m_snapshot = DepthSnapshot();
    m_verificationFrames = 0;
    DepthCandidateCollector::Get().Clear();
}

void DepthLockManager::OnFrameEnd(uint64_t frameNumber, uint32_t deviceGeneration, uint32_t swapchainGeneration, uint32_t swapchainWidth, uint32_t swapchainHeight) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // If device generation or swapchain generation changed since last snapshot, we might be lost
    if (m_snapshot.IsValid() && (m_snapshot.deviceGeneration != deviceGeneration || m_snapshot.swapchainGeneration != swapchainGeneration)) {
        m_state = DepthLockState::RECOVERING;
        m_snapshot = DepthSnapshot(); // invalidate
        m_verificationFrames = 0;
    }

    auto candidates = DepthCandidateCollector::Get().GetCandidatesSnapshot();
    
    // Process pipeline
    for (auto& cand : candidates) {
        DepthClassifier::Classify(cand);
        DepthValidator::Validate(cand, swapchainWidth, swapchainHeight);
        DepthDeltaTracker::Track(cand, frameNumber);
    }
    
    DepthRankingEngine::Rank(candidates);
    
    DepthCandidate* bestCandidate = nullptr;
    if (!candidates.empty() && candidates[0].valid && candidates[0].confidence > 0.0f) {
        bestCandidate = &candidates[0];
    }

    // State machine logic
    switch (m_state) {
        case DepthLockState::UNLOCKED:
        case DepthLockState::SEARCHING:
        case DepthLockState::LOST:
        case DepthLockState::RECOVERING:
            if (bestCandidate && bestCandidate->confidence > 50.0f) {
                m_state = DepthLockState::VERIFYING;
                m_verificationFrames = 1;
                
                m_snapshot.identity = bestCandidate->identity;
                m_snapshot.confidence = bestCandidate->confidence;
                m_snapshot.convention = ReverseZDetector::Detect(*bestCandidate, bestCandidate->lastClearDepth);
                m_snapshot.deviceGeneration = deviceGeneration;
                m_snapshot.swapchainGeneration = swapchainGeneration;
                m_snapshot.depthGeneration = m_currentDepthGeneration++;
            } else {
                m_state = DepthLockState::SEARCHING;
            }
            break;
            
        case DepthLockState::VERIFYING:
        case DepthLockState::REVERIFYING:
            if (bestCandidate && bestCandidate->identity == m_snapshot.identity) {
                m_verificationFrames++;
                m_snapshot.confidence = bestCandidate->confidence;
                m_snapshot.convention = ReverseZDetector::Detect(*bestCandidate, bestCandidate->lastClearDepth);
                
                if (m_verificationFrames >= REQUIRED_VERIFICATION_FRAMES) {
                    m_state = DepthLockState::LOCKED;
                }
            } else {
                // Lost tracking or best candidate changed
                m_state = DepthLockState::SEARCHING;
                m_snapshot = DepthSnapshot();
            }
            break;
            
        case DepthLockState::LOCKED:
            if (bestCandidate && bestCandidate->identity == m_snapshot.identity) {
                m_snapshot.confidence = bestCandidate->confidence;
                // Still locked
                if (m_snapshot.confidence < 80.0f) {
                    m_state = DepthLockState::REVERIFYING;
                }
            } else {
                // Primary depth buffer changed or got destroyed
                m_state = DepthLockState::LOST;
                m_snapshot = DepthSnapshot();
            }
            break;
    }
}

DepthSnapshot DepthLockManager::GetSnapshot() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_snapshot;
}

} // namespace vrinject
