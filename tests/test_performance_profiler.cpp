#include <gtest/gtest.h>
#include "core/performance_profiler.h"
#include <thread>
#include <chrono>

using namespace vrinject;

TEST(PerformanceProfilerTest, BasicTiming) {
    PerformanceProfiler profiler(10);
    
    profiler.BeginSegment(CpuSegment::CameraDiscovery);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    profiler.EndSegment(CpuSegment::CameraDiscovery);
    
    auto stats = profiler.GetStats(CpuSegment::CameraDiscovery);
    EXPECT_GE(stats.averageMs, 4.0);
}
