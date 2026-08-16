#include <gtest/gtest.h>
#include "heuristics/camera_lock_manager.h"

using namespace vrinject;

namespace {

// FrameCoordinator hands the lock manager the frame's colour resource each frame via
// SetResourceContext(). CameraSnapshot::IsValid() requires it, so a test that wants a
// valid snapshot must supply one too -- otherwise it asserts something unreachable.
GraphicsResourceIdentity TestBackBuffer() {
    GraphicsResourceIdentity identity;
    identity.backend = GraphicsBackend::DX11;
    identity.nativeHandle = reinterpret_cast<void*>(0xC0FFEE);  // opaque; never dereferenced
    identity.width = 1920;
    identity.height = 1080;
    return identity;
}

} // namespace

TEST(CameraLockTest, InitialLockProgression) {
    CameraLockManager::Get().Reset();
    EXPECT_EQ(CameraLockManager::Get().GetState(), CameraLockState::UNLOCKED);

    CameraLockManager::Get().SetResourceContext(TestBackBuffer());

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

// Reset() runs on device loss, where the cached backbuffer handle becomes dangling.
// Guards that it is dropped rather than stamped into the next epoch's snapshots.
TEST(CameraLockTest, ResetClearsStaleResourceContext) {
    CameraLockManager::Get().Reset();
    CameraLockManager::Get().SetResourceContext(TestBackBuffer());

    CameraCandidate c = {};
    c.id = 100;
    c.valid = true;
    c.confidence = 0.6f;
    for (uint64_t frame = 1; frame <= 4; ++frame) {
        CameraLockManager::Get().Update(c, frame);
    }
    ASSERT_TRUE(CameraLockManager::Get().GetSnapshot().IsValid());

    // Simulate device loss: the old backbuffer pointer must not survive.
    CameraLockManager::Get().Reset();
    EXPECT_EQ(CameraLockManager::Get().GetSnapshot().resourceIdentity.nativeHandle, nullptr);

    // Re-locking without a fresh context must not resurrect the stale handle.
    for (uint64_t frame = 5; frame <= 8; ++frame) {
        CameraLockManager::Get().Update(c, frame);
    }
    EXPECT_FALSE(CameraLockManager::Get().GetSnapshot().IsValid());
}
