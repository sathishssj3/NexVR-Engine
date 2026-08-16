#include <gtest/gtest.h>

#include "core/frame_graph_profiler.h"

using namespace vrinject;

TEST(FrameGraphProfiler, ProducesImmutableFrameGraphSnapshot) {
    FrameGraphProfiler profiler;
    profiler.BeginFrame(42);
    profiler.AddEvent(FrameGraphEventType::ComputeDispatch, "stereo", 1.25, 2);
    profiler.AddEvent(FrameGraphEventType::Copy, "xr-copy", 0.50, 1);

    auto snapshot = profiler.EndFrame();

    EXPECT_EQ(snapshot.frameIndex, 42u);
    EXPECT_EQ(snapshot.eventCount, 2u);
    EXPECT_EQ(snapshot.computeDispatchCount, 1u);
    EXPECT_EQ(snapshot.copyCount, 1u);
    EXPECT_DOUBLE_EQ(snapshot.totalGpuMs, 1.75);
}
