#include <gtest/gtest.h>
#include "../src/core/camera_delta_tracker.h"

using namespace vrinject;

TEST(CameraDeltaTrackerTest, TracksStaticCamera) {
    CameraCandidate prev = {};
    prev.temporalScore = 0.5f;
    prev.viewProjection.m[0][0] = 1.0f; // some matrix
    
    CameraCandidate curr = prev; // exactly same matrix
    
    CameraDeltaTracker::UpdateDelta(prev, curr);
    EXPECT_LT(curr.temporalScore, 0.5f); // score drops for completely static
}

TEST(CameraDeltaTrackerTest, TracksMovingCamera) {
    CameraCandidate prev = {};
    prev.temporalScore = 0.5f;
    prev.viewProjection.m[0][0] = 1.0f;
    
    CameraCandidate curr = prev;
    curr.viewProjection.m[0][0] = 1.1f; // small movement
    
    CameraDeltaTracker::UpdateDelta(prev, curr);
    EXPECT_GT(curr.temporalScore, 0.5f); // score increases for motion
}
