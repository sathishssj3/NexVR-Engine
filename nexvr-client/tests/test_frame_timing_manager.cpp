#include <gtest/gtest.h>

#include "rendering/frame_timing_manager.h"

using namespace vrinject;

TEST(FrameTimingManager, ComputesRollingTimingSnapshot) {
    FrameTimingManager manager(11.111);

    auto snapshot = manager.Update(4.0, 5.0, 1.0, false);

    EXPECT_EQ(snapshot.frameIndex, 1u);
    EXPECT_DOUBLE_EQ(snapshot.cpuFrameMs, 4.0);
    EXPECT_DOUBLE_EQ(snapshot.gpuFrameMs, 5.0);
    EXPECT_DOUBLE_EQ(snapshot.motionToPhotonMs, 10.0);
    EXPECT_FALSE(snapshot.overBudget);
}

TEST(FrameTimingManager, RejectsNegativeInputsByClampingToZero) {
    FrameTimingManager manager;

    auto snapshot = manager.Update(-10.0, -2.0, -1.0, true);

    EXPECT_DOUBLE_EQ(snapshot.cpuFrameMs, 0.0);
    EXPECT_DOUBLE_EQ(snapshot.gpuFrameMs, 0.0);
    EXPECT_DOUBLE_EQ(snapshot.predictionErrorMs, 0.0);
    EXPECT_DOUBLE_EQ(snapshot.droppedFrameRatio, 1.0);
}
