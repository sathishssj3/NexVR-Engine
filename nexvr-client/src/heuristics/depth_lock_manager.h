#pragma once
#include "heuristics/depth_snapshot.h"
#include "heuristics/depth_candidate_collector.h"
#include <mutex>
#include <vector>

namespace vrinject {

enum class DepthLockState {
    UNLOCKED,
    SEARCHING,
    CANDIDATE_FOUND,
    VERIFYING,
    LOCKED,
    REVERIFYING,
    LOST,
    RECOVERING
};

class DepthLockManager {
public:
    DepthLockManager() = default;
    ~DepthLockManager() = default;

    void OnFrameEnd(uint64_t frameNumber, uint32_t deviceGeneration, uint32_t swapchainGeneration, uint32_t swapchainWidth, uint32_t swapchainHeight);
    
    DepthSnapshot GetSnapshot() const;
    DepthLockState GetState() const { return m_state; }

    void OnDeviceLost();

private:

    DepthLockState m_state = DepthLockState::UNLOCKED;
    DepthSnapshot m_snapshot;
    
    uint32_t m_currentDepthGeneration = 1;
    uint32_t m_verificationFrames = 0;
    const uint32_t REQUIRED_VERIFICATION_FRAMES = 5;

    mutable std::mutex m_mutex;
};

} // namespace vrinject
