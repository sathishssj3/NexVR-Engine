#include "performance_profiler.h"

namespace vrinject {

PerformanceProfiler::PerformanceProfiler(size_t windowSize)
    : m_windowSize(windowSize) {
    for (auto& seg : m_segments) {
        seg.history.resize(m_windowSize, 0.0);
    }
}

void PerformanceProfiler::BeginSegment(CpuSegment segment) {
    auto& seg = m_segments[static_cast<size_t>(segment)];
    seg.start = std::chrono::high_resolution_clock::now();
}

void PerformanceProfiler::EndSegment(CpuSegment segment) {
    auto end = std::chrono::high_resolution_clock::now();
    
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& seg = m_segments[static_cast<size_t>(segment)];
    
    std::chrono::duration<double, std::milli> elapsed = end - seg.start;
    seg.lastMs = elapsed.count();
    
    seg.history[seg.head] = seg.lastMs;
    seg.head++;
    if (seg.head >= m_windowSize) {
        seg.head = 0;
        seg.full = true;
    }
    
    seg.statsDirty = true;
}

double PerformanceProfiler::GetLastLatency(CpuSegment segment) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_segments[static_cast<size_t>(segment)].lastMs;
}

PerformanceStats PerformanceProfiler::GetStats(CpuSegment segment) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    CalculateStats(segment);
    return m_segments[static_cast<size_t>(segment)].cachedStats;
}

void PerformanceProfiler::CalculateStats(CpuSegment segment) const {
    auto& seg = m_segments[static_cast<size_t>(segment)];
    if (!seg.statsDirty) return;

    size_t count = seg.full ? m_windowSize : seg.head;
    if (count == 0) {
        seg.cachedStats = {};
        seg.statsDirty = false;
        return;
    }

    std::vector<double> sorted(count);
    for (size_t i = 0; i < count; ++i) {
        sorted[i] = seg.history[i];
    }
    std::sort(sorted.begin(), sorted.end());

    double sum = 0.0;
    for (double val : sorted) {
        sum += val;
    }

    seg.cachedStats.averageMs = sum / count;
    seg.cachedStats.minMs = sorted.front();
    seg.cachedStats.maxMs = sorted.back();

    auto percentile = [&](double p) {
        if (count == 1) return sorted[0];
        double rank = p * (count - 1);
        size_t lower = static_cast<size_t>(rank);
        size_t upper = lower + 1;
        double weight = rank - lower;
        if (upper >= count) return sorted.back();
        return sorted[lower] * (1.0 - weight) + sorted[upper] * weight;
    };

    seg.cachedStats.p50Ms = percentile(0.50);
    seg.cachedStats.p95Ms = percentile(0.95);
    seg.cachedStats.p99Ms = percentile(0.99);
    seg.cachedStats.p99_9Ms = percentile(0.999);

    seg.statsDirty = false;
}

} // namespace vrinject
