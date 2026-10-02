#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace vrinject {

struct FrameTimingSnapshot {
    double cpuFrameMs = 0.0;
    double gpuFrameMs = 0.0;
    double motionToPhotonMs = 0.0;
    double predictionErrorMs = 0.0;
    double targetFrameMs = 11.111;
    double droppedFrameRatio = 0.0;
    uint64_t frameIndex = 0;
    bool overBudget = false;
};

class FrameTimingManager {
public:
    explicit FrameTimingManager(double targetFrameMs = 11.111);

    FrameTimingSnapshot Update(double cpuFrameMs,
                               double gpuFrameMs,
                               double predictionErrorMs,
                               bool frameDropped);

    FrameTimingSnapshot GetSnapshot() const { return m_snapshot; }
    void Reset();

private:
    static constexpr size_t WINDOW_SIZE = 120;

    double Average(const std::array<double, WINDOW_SIZE>& samples, size_t count) const;

    double m_targetFrameMs = 11.111;
    uint64_t m_frameIndex = 0;
    size_t m_sampleCount = 0;
    size_t m_nextSample = 0;
    uint64_t m_droppedFrames = 0;
    std::array<double, WINDOW_SIZE> m_cpuSamples{};
    std::array<double, WINDOW_SIZE> m_gpuSamples{};
    std::array<double, WINDOW_SIZE> m_predictionSamples{};
    FrameTimingSnapshot m_snapshot{};
};

} // namespace vrinject
