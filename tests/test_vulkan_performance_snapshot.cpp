#include <gtest/gtest.h>

#include "src/core/vulkan_snapshot_validator.h"
#include "src/rendering/frame_timing_manager.h"

#include <string>

using namespace vrinject;
using namespace vrinject::vulkan;

TEST(VulkanPerformanceSnapshot, RejectsImpossibleFrameTiming) {
    FrameTimingSnapshot timing{};
    timing.cpuFrameMs = -1.0;
    timing.targetFrameMs = 11.111;

    std::string error;
    EXPECT_FALSE(VulkanSnapshotValidator::ValidateFrameTiming(timing, error));
    EXPECT_EQ(error, "Frame timing contains negative duration.");
}

TEST(VulkanPerformanceSnapshot, AcceptsPlausiblePerformanceSnapshot) {
    PerformanceSnapshot snapshot{};
    snapshot.cpuMs = 4.0;
    snapshot.gpuMs = 5.0;
    snapshot.stereoLatencyMs = 9.0;
    snapshot.queueUtilization = 0.5;

    std::string error;
    EXPECT_TRUE(VulkanSnapshotValidator::ValidatePerformanceSnapshot(snapshot, error));
}
