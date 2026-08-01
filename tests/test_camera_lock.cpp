#include <gtest/gtest.h>
#include "../src/core/camera_lock_manager.h"

using namespace vrinject;

TEST(CameraLockTest, InitialLockProgression) {
    CameraLockManager::Get().Reset();
    EXPECT_EQ(CameraLockManager::Get().GetState(), CameraLockState::UNLOCKED);

    CameraCandidate c = {};
    c.id = 100;
    c.valid = true;
    c.confidence = 0.6f;

    // Frame 1 -> SEARCHING
    CameraLockManager::Get().Update(c, 1);
    EXPECT_EQ(CameraLockManager::Get().GetState(), CameraLockState::SEARCHING);

    // Frame 2 -> CANDIDATE_FOUND
    CameraLockManager::Get().Update(c, 2);
    EXPECT_EQ(CameraLockManager::Get().GetState(), CameraLockState::CANDIDATE_FOUND);

    // Frame 3 -> VERIFYING
    CameraLockManager::Get().Update(c, 3);
    EXPECT_EQ(CameraLockManager::Get().GetState(), CameraLockState::VERIFYING);

    // Frame 4 -> LOCKED
    CameraLockManager::Get().Update(c, 4);
    EXPECT_EQ(CameraLockManager::Get().GetState(), CameraLockState::LOCKED);
    
    // Check Snapshot
    CameraSnapshot s = CameraLockManager::Get().GetSnapshot();
    EXPECT_TRUE(s.IsValid());
}

TEST(CameraLockTest, RelockAfterConfidenceDrop) {
    CameraLockManager::Get().Reset();
    
    CameraCandidate c = {};
    c.id = 100;
    c.valid = true;
    c.confidence = 0.6f;

    // Fast-forward to LOCKED
    CameraLockManager::Get().Update(c, 1);
    CameraLockManager::Get().Update(c, 2);
    CameraLockManager::Get().Update(c, 3);
    CameraLockManager::Get().Update(c, 4);
    EXPECT_EQ(CameraLockManager::Get().GetState(), CameraLockState::LOCKED);

    // Drop confidence
    c.confidence = 0.4f;
    CameraLockManager::Get().Update(c, 5);
    EXPECT_EQ(CameraLockManager::Get().GetState(), CameraLockState::REVERIFYING);

    // Drop more -> RELOCKING
    c.confidence = 0.2f;
    CameraLockManager::Get().Update(c, 6);
    EXPECT_EQ(CameraLockManager::Get().GetState(), CameraLockState::RELOCKING);

    // Recovery
    c.confidence = 0.8f;
    CameraLockManager::Get().Update(c, 7);
    EXPECT_EQ(CameraLockManager::Get().GetState(), CameraLockState::LOCKED);
}
