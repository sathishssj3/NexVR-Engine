#include "frame_graph_profiler.h"

#include <algorithm>
#include <cstring>

namespace vrinject {
namespace {

void CopyName(char* dest, size_t destSize, const char* src) {
    if (!dest || destSize == 0) return;
#ifdef _WIN32
    strncpy_s(dest, destSize, src ? src : "", _TRUNCATE);
#else
    std::strncpy(dest, src ? src : "", destSize - 1);
    dest[destSize - 1] = '\0';
#endif
}

} // namespace

void FrameGraphProfiler::BeginFrame(uint64_t frameIndex) {
    m_snapshot = {};
    m_snapshot.frameIndex = frameIndex;
    m_active = true;
}

void FrameGraphProfiler::AddEvent(FrameGraphEventType type,
                                  const char* name,
                                  double gpuMs,
                                  uint32_t barrierCount) {
    if (!m_active || m_snapshot.eventCount >= m_snapshot.events.size()) return;

    auto& event = m_snapshot.events[m_snapshot.eventCount++];
    event.type = type;
    CopyName(event.name, sizeof(event.name), name);
    event.gpuMs = std::max(gpuMs, 0.0);
    event.barrierCount = barrierCount;
    m_snapshot.totalGpuMs += event.gpuMs;

    switch (type) {
        case FrameGraphEventType::RenderPass: m_snapshot.renderPassCount++; break;
        case FrameGraphEventType::ComputeDispatch: m_snapshot.computeDispatchCount++; break;
        case FrameGraphEventType::Copy: m_snapshot.copyCount++; break;
        case FrameGraphEventType::Synchronization: m_snapshot.synchronizationCount++; break;
    }
}

FrameGraphSnapshot FrameGraphProfiler::EndFrame() {
    m_active = false;
    return m_snapshot;
}

} // namespace vrinject
