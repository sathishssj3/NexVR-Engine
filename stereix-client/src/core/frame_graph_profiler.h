#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace vrinject {

enum class FrameGraphEventType {
    RenderPass,
    ComputeDispatch,
    Copy,
    Synchronization
};

struct FrameGraphEvent {
    FrameGraphEventType type = FrameGraphEventType::RenderPass;
    char name[48] = {};
    double gpuMs = 0.0;
    uint32_t barrierCount = 0;
};

struct FrameGraphSnapshot {
    uint64_t frameIndex = 0;
    std::array<FrameGraphEvent, 64> events{};
    size_t eventCount = 0;
    uint32_t renderPassCount = 0;
    uint32_t computeDispatchCount = 0;
    uint32_t copyCount = 0;
    uint32_t synchronizationCount = 0;
    double totalGpuMs = 0.0;
};

struct PerformanceSnapshot {
    double cpuMs = 0.0;
    double gpuMs = 0.0;
    double stereoLatencyMs = 0.0;
    double queueUtilization = 0.0;
    uint64_t droppedFrames = 0;
    uint32_t adaptiveQualityLevel = 0;
    uint64_t pipelineCacheHits = 0;
    uint64_t barrierCount = 0;
    uint64_t dispatchCount = 0;
};

class FrameGraphProfiler {
public:
    void BeginFrame(uint64_t frameIndex);
    void AddEvent(FrameGraphEventType type, const char* name, double gpuMs, uint32_t barrierCount = 0);
    FrameGraphSnapshot EndFrame();
    FrameGraphSnapshot GetSnapshot() const { return m_snapshot; }

private:
    FrameGraphSnapshot m_snapshot{};
    bool m_active = false;
};

} // namespace vrinject
