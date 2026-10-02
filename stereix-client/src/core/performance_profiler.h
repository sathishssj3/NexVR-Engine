#pragma once

#include <chrono>
#include <array>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <mutex>

namespace vrinject {

enum class CpuSegment {
    Dx11Hook = 0,
    CameraDiscovery,
    DepthDiscovery,
    StereoGeneration,
    StereoRendering,
    OpenXrSubmission,
    EntireFrame,
    COUNT
};

struct PerformanceStats {
    double averageMs;
    double minMs;
    double maxMs;
    double p50Ms;
    double p95Ms;
    double p99Ms;
    double p99_9Ms;
};

class PerformanceProfiler {
public:
    PerformanceProfiler(size_t windowSize = 1000);

    void BeginSegment(CpuSegment segment);
    void EndSegment(CpuSegment segment);

    PerformanceStats GetStats(CpuSegment segment) const;
    double GetLastLatency(CpuSegment segment) const;

private:
    struct SegmentData {
        std::chrono::time_point<std::chrono::high_resolution_clock> start;
        std::vector<double> history; // Circular buffer
        size_t head = 0;
        bool full = false;
        double lastMs = 0.0;
        
        // Cached stats to avoid recalculating on every GetStats if not needed
        mutable PerformanceStats cachedStats = {};
        mutable bool statsDirty = true;
    };

    size_t m_windowSize;
    std::array<SegmentData, static_cast<size_t>(CpuSegment::COUNT)> m_segments;
    mutable std::mutex m_mutex;

    void CalculateStats(CpuSegment segment) const;
};

class ScopedCpuTimer {
public:
    ScopedCpuTimer(PerformanceProfiler* profiler, CpuSegment segment)
        : m_profiler(profiler), m_segment(segment) {
        if (m_profiler) {
            m_profiler->BeginSegment(m_segment);
        }
    }

    ~ScopedCpuTimer() {
        if (m_profiler) {
            m_profiler->EndSegment(m_segment);
        }
    }

private:
    PerformanceProfiler* m_profiler;
    CpuSegment m_segment;
};

} // namespace vrinject
