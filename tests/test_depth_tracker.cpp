#include <gtest/gtest.h>
#include "../src/core/depth_delta_tracker.h"

using namespace vrinject;

TEST(DepthTracker, TracksTemporalAndPatterns) {
    DepthCandidate cand;
    cand.identity.creationGeneration = 10;
    cand.frameSeen = 20;
    cand.lastFrameSeen = 19;
    cand.bindCount = 5;
    cand.clearCount = 1;
    
    DepthDeltaTracker::Track(cand, 21);
    
    EXPECT_GT(cand.lifetimeScore, 0.0f);
    EXPECT_EQ(cand.clearPatternScore, 1.0f); // 1 clear per frame is ideal
    EXPECT_EQ(cand.bindFrequencyScore, 0.5f); // 5 / 10
}
