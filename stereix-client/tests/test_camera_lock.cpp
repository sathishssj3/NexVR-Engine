#include <gtest/gtest.h>
#include "heuristics/camera_lock_manager.h"
#include "core/subsystem_context.h"
#include "core/config_manager.h"
#include "core/logger.h"

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

class TestLogger : public vrinject::ILogger {
public:
    void Init(const std::string& logPath) override {}
    void Log(vrinject::ILogger::Level level, const char* file, int line, const char* fmt, ...) override {}
    void Shutdown() override {}
};

class CameraLockTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto config = std::make_shared<ConfigManager>();
        auto logger = std::make_shared<TestLogger>();
        SubsystemContext::Get().Initialize(logger, config);
    }
    
    void TearDown() override {
        SubsystemContext::Get().Shutdown();
    }
};

TEST_F(CameraLockTest, BasicStateTransitions) {
    CameraLockManager* mgr = SubsystemContext::Get().GetCameraLockManager();
    mgr->Reset();
    EXPECT_EQ(mgr->GetState(), CameraLockState::UNLOCKED);
    
    mgr->SetResourceContext(TestBackBuffer());

    CameraCandidate c = {};
    c.id = 100;
    c.valid = true;
    c.confidence = 0.6f;

    // Frame 1 -> SEARCHING
    mgr->Update(c, 1);
    EXPECT_EQ(mgr->GetState(), CameraLockState::SEARCHING);
    
    // Simulate candidate found
    mgr->Update(c, 2);
    EXPECT_EQ(mgr->GetState(), CameraLockState::CANDIDATE_FOUND);
    
    // Simulating tracking
    mgr->Update(c, 3);
    EXPECT_EQ(mgr->GetState(), CameraLockState::VERIFYING);
    
    // Simulating lock
    mgr->Update(c, 4);
    EXPECT_EQ(mgr->GetState(), CameraLockState::LOCKED);
    
    // Verify snapshot
    CameraSnapshot s = mgr->GetSnapshot();
    EXPECT_TRUE(s.IsValid());
}

TEST_F(CameraLockTest, RelockAfterConfidenceDrop) {
    CameraLockManager* mgr = SubsystemContext::Get().GetCameraLockManager();
    mgr->Reset();
    
    CameraCandidate c = {};
    c.id = 100;
    c.valid = true;
    c.confidence = 0.6f;

    // Fast-forward to LOCKED
    mgr->Update(c, 1);
    mgr->Update(c, 2);
    mgr->Update(c, 3);
    mgr->Update(c, 4);
    EXPECT_EQ(mgr->GetState(), CameraLockState::LOCKED);
    
    // Lower confidence slightly
    c.confidence = 0.4f;
    mgr->Update(c, 5);
    EXPECT_EQ(mgr->GetState(), CameraLockState::REVERIFYING);
    
    // Drop more -> RELOCKING
    c.confidence = 0.2f;
    mgr->Update(c, 6);
    EXPECT_EQ(mgr->GetState(), CameraLockState::RELOCKING);
    
    // Recovery
    c.confidence = 0.8f;
    mgr->Update(c, 7);
    EXPECT_EQ(mgr->GetState(), CameraLockState::LOCKED);
}

// Reset() runs on device loss, where the cached backbuffer handle becomes dangling.
// Guards that it is dropped rather than stamped into the next epoch's snapshots.
TEST_F(CameraLockTest, ResetClearsStaleResourceContext) {
    CameraLockManager* mgr = SubsystemContext::Get().GetCameraLockManager();
    mgr->Reset();
    mgr->SetResourceContext(TestBackBuffer());

    CameraCandidate c = {};
    c.id = 100;
    c.valid = true;
    c.confidence = 0.6f;
    for (int frame = 1; frame <= 4; ++frame) {
        mgr->Update(c, frame);
    }
    ASSERT_TRUE(mgr->GetSnapshot().IsValid());
    
    // Simulate device loss: the old backbuffer pointer must not survive.
    mgr->Reset();
    EXPECT_EQ(mgr->GetSnapshot().resourceIdentity.nativeHandle, nullptr);

    // Re-locking without a fresh context must not resurrect the stale handle.
    for (int frame = 5; frame <= 8; ++frame) {
        mgr->Update(c, frame);
    }
    EXPECT_FALSE(mgr->GetSnapshot().IsValid());
}
