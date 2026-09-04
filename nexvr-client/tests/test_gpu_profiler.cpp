#include <gtest/gtest.h>
#include "core/gpu_profiler.h"

using namespace vrinject;

TEST(GpuProfilerTest, BasicInstantiation) {
    GpuProfiler profiler;
    uint64_t frameIndex;
    std::array<GpuTimingResult, static_cast<uint32_t>(GpuSegment::COUNT)> results;
    EXPECT_FALSE(profiler.TryGetTimings(frameIndex, results));
}
