#pragma once

#include "runtime_state_monitor.h"
#include "performance_profiler.h"
#include "gpu_profiler.h"

namespace vrinject {

class RuntimeDashboard {
public:
    RuntimeDashboard();

    void Update(
        const RuntimeHealthReport& health,
        const PerformanceProfiler& cpuProfiler,
        const GpuProfiler& gpuProfiler
    );

    void PrintToConsole() const;
    void LogToSystem() const;

private:
    RuntimeHealthReport m_lastHealth;
    PerformanceStats m_cpuStats[static_cast<size_t>(CpuSegment::COUNT)];
    
    uint64_t m_lastGpuFrame = 0;
    std::array<GpuTimingResult, static_cast<uint32_t>(GpuSegment::COUNT)> m_gpuResults;

    uint64_t m_lastPrintTimeMs = 0;
};

} // namespace vrinject
