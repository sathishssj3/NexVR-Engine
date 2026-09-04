#include <gtest/gtest.h>

#include "rendering/vulkan_gpu_profiler.h"

using namespace vrinject::vulkan;

TEST(VulkanGpuProfiler, TracksMovingAveragesAndUtilization) {
    VulkanGpuProfiler profiler(10.0);

    profiler.RecordSample({2.0, 3.0, 1.0, 0.5});
    profiler.RecordSample({4.0, 5.0, 1.0, 0.0});
    auto snapshot = profiler.GetSnapshot();

    EXPECT_DOUBLE_EQ(snapshot.dispatchMs, 3.0);
    EXPECT_DOUBLE_EQ(snapshot.copyMs, 4.0);
    EXPECT_DOUBLE_EQ(snapshot.barrierMs, 1.0);
    EXPECT_DOUBLE_EQ(snapshot.queueIdleMs, 0.25);
    EXPECT_DOUBLE_EQ(snapshot.gpuUtilization, 0.8);
    EXPECT_EQ(snapshot.sampleCount, 2u);
}
