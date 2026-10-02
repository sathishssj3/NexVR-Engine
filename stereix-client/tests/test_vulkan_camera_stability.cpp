#include <gtest/gtest.h>
#include "heuristics/temporal_camera_filter.h"
#include "core/subsystem_context.h"
#include <cmath>

using namespace vrinject;

class TemporalCameraFilterTest : public ::testing::Test {
protected:
    void SetUp() override {
        SubsystemContext::Get().Initialize(nullptr, nullptr);
        SubsystemContext::Get().GetTemporalCameraFilter()->Reset();
    }

    void TearDown() override {
        SubsystemContext::Get().Shutdown();
    }
};

TEST_F(TemporalCameraFilterTest, BasicLockAndCoast) {
    auto filter = SubsystemContext::Get().GetTemporalCameraFilter();

    CameraCandidate candidate = {};
    candidate.valid = true;
    candidate.id = 100;
    candidate.confidence = 1.0f;

    // Searching... (needs 3 frames)
    filter->Update(candidate, 1);
    EXPECT_FALSE(filter->GetStableCandidate().valid);

    filter->Update(candidate, 2);
    EXPECT_FALSE(filter->GetStableCandidate().valid);

    filter->Update(candidate, 3);
    EXPECT_TRUE(filter->GetStableCandidate().valid);
    EXPECT_EQ(filter->GetStableCandidate().id, 100);

    // Coasting
    CameraCandidate invalid = {};
    invalid.valid = false;
    
    // Coast for 5 frames
    for (int i = 4; i <= 8; ++i) {
        filter->Update(invalid, i);
        EXPECT_TRUE(filter->GetStableCandidate().valid); // still returns last known
    }

    // Exceed coasting limit (10 frames)
    for (int i = 9; i <= 15; ++i) {
        filter->Update(invalid, i);
    }
    EXPECT_FALSE(filter->GetStableCandidate().valid); // lost
}

TEST_F(TemporalCameraFilterTest, JumpRejection) {
    auto filter = SubsystemContext::Get().GetTemporalCameraFilter();

    // valid=true is required: TemporalCameraFilter::Update() gates every transition on
    // "bestCandidate.valid && id != 0", and CameraCandidate has no default member
    // initialisers, so "= {}" leaves valid==false and the filter never leaves SEARCHING.
    CameraCandidate c1 = {}; c1.valid = true; c1.id = 1; c1.confidence = 1.0f;
    CameraCandidate c2 = {}; c2.valid = true; c2.id = 2; c2.confidence = 1.0f;

    filter->Update(c1, 1);
    filter->Update(c2, 2); // Jumped to c2! Should lock counter reset?
    filter->Update(c2, 3);
    filter->Update(c2, 4);

    EXPECT_TRUE(filter->GetStableCandidate().valid);
    EXPECT_EQ(filter->GetStableCandidate().id, 2);
}
