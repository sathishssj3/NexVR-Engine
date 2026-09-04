#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace vrinject {
namespace vulkan {

struct VulkanGpuProfilerSample {
    double dispatchMs = 0.0;
    double copyMs = 0.0;
    double barrierMs = 0.0;
    double queueIdleMs = 0.0;
};

struct VulkanGpuProfilerSnapshot {
    double dispatchMs = 0.0;
    double copyMs = 0.0;
    double barrierMs = 0.0;
    double queueIdleMs = 0.0;
    double gpuUtilization = 0.0;
    uint64_t sampleCount = 0;
    bool timestampQueriesSupported = false;
};

class VulkanGpuProfiler {
public:
    explicit VulkanGpuProfiler(double targetFrameMs = 11.111);
    ~VulkanGpuProfiler();

    bool Initialize(VkDevice device, uint32_t queryCount = 16);
    void Shutdown();

    void RecordSample(const VulkanGpuProfilerSample& sample);
    VulkanGpuProfilerSnapshot GetSnapshot() const { return m_snapshot; }
    void Reset();

private:
    static constexpr size_t WINDOW_SIZE = 120;

    double Average(const std::array<double, WINDOW_SIZE>& samples, size_t count) const;
    void RecomputeSnapshot();

    VkDevice m_device = VK_NULL_HANDLE;
    VkQueryPool m_queryPool = VK_NULL_HANDLE;
    double m_targetFrameMs = 11.111;
    size_t m_nextSample = 0;
    size_t m_windowCount = 0;
    uint64_t m_totalSamples = 0;
    std::array<double, WINDOW_SIZE> m_dispatchSamples{};
    std::array<double, WINDOW_SIZE> m_copySamples{};
    std::array<double, WINDOW_SIZE> m_barrierSamples{};
    std::array<double, WINDOW_SIZE> m_idleSamples{};
    VulkanGpuProfilerSnapshot m_snapshot{};
};

} // namespace vulkan
} // namespace vrinject
