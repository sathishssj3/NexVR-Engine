#include "frame_timing_manager.h"

#include <algorithm>

namespace vrinject {

FrameTimingManager::FrameTimingManager(double targetFrameMs)
    : m_targetFrameMs(targetFrameMs > 0.0 ? targetFrameMs : 11.111) {
    m_snapshot.targetFrameMs = m_targetFrameMs;
}

FrameTimingSnapshot FrameTimingManager::Update(double cpuFrameMs,
                                               double gpuFrameMs,
                                               double predictionErrorMs,
                                               bool frameDropped) {
    cpuFrameMs = std::max(cpuFrameMs, 0.0);
    gpuFrameMs = std::max(gpuFrameMs, 0.0);
    predictionErrorMs = std::max(predictionErrorMs, 0.0);

    m_cpuSamples[m_nextSample] = cpuFrameMs;
    m_gpuSamples[m_nextSample] = gpuFrameMs;
    m_predictionSamples[m_nextSample] = predictionErrorMs;
    m_nextSample = (m_nextSample + 1) % WINDOW_SIZE;
    m_sampleCount = std::min(m_sampleCount + 1, WINDOW_SIZE);
    m_frameIndex++;
    if (frameDropped) {
        m_droppedFrames++;
    }

    m_snapshot.frameIndex = m_frameIndex;
    m_snapshot.cpuFrameMs = Average(m_cpuSamples, m_sampleCount);
    m_snapshot.gpuFrameMs = Average(m_gpuSamples, m_sampleCount);
    m_snapshot.predictionErrorMs = Average(m_predictionSamples, m_sampleCount);
    m_snapshot.motionToPhotonMs =
        m_snapshot.cpuFrameMs + m_snapshot.gpuFrameMs + m_snapshot.predictionErrorMs;
    m_snapshot.targetFrameMs = m_targetFrameMs;
    m_snapshot.overBudget =
        m_snapshot.cpuFrameMs > m_targetFrameMs || m_snapshot.gpuFrameMs > m_targetFrameMs;
    m_snapshot.droppedFrameRatio =
        m_frameIndex == 0 ? 0.0 : static_cast<double>(m_droppedFrames) / static_cast<double>(m_frameIndex);

    return m_snapshot;
}

void FrameTimingManager::Reset() {
    m_frameIndex = 0;
    m_sampleCount = 0;
    m_nextSample = 0;
    m_droppedFrames = 0;
    m_cpuSamples.fill(0.0);
    m_gpuSamples.fill(0.0);
    m_predictionSamples.fill(0.0);
    m_snapshot = {};
    m_snapshot.targetFrameMs = m_targetFrameMs;
}

double FrameTimingManager::Average(const std::array<double, WINDOW_SIZE>& samples, size_t count) const {
    if (count == 0) return 0.0;

    double total = 0.0;
    for (size_t i = 0; i < count; ++i) {
        total += samples[i];
    }
    return total / static_cast<double>(count);
}

} // namespace vrinject
