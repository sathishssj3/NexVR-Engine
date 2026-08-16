#include <gtest/gtest.h>
#include "heuristics/depth_lock_manager.h"
#include "heuristics/depth_candidate_collector.h"

using namespace vrinject;

TEST(DepthLockManager, StateMachineWorks) {
    DepthLockManager& manager = DepthLockManager::Get();
    manager.OnDeviceLost(); // Reset
    
    EXPECT_EQ(manager.GetState(), DepthLockState::LOST);
    
    // Simulate candidate
    void* dummyPtr = (void*)0x1234;
    DepthCandidateCollector::Get().OnDepthSurfaceCreated(dummyPtr, 1920, 1080, 40, 1, 1, 1, 1); // DXGI_FORMAT_D32_FLOAT = 40
    DepthCandidateCollector::Get().OnOMSetRenderTargets(dummyPtr);
    DepthCandidateCollector::Get().OnClearDepthStencilView(dummyPtr, 1.0f);
    
    for (int i = 0; i < 5; ++i) {
        manager.OnFrameEnd(i, 1, 1, 1920, 1080);
        if (i == 0) {
            EXPECT_EQ(manager.GetState(), DepthLockState::VERIFYING);
        }
    }
    
    EXPECT_EQ(manager.GetState(), DepthLockState::LOCKED);
    EXPECT_EQ(manager.GetSnapshot().convention, DepthConvention::Standard);
}
