#include "rendering/vulkan_gpu_profiler.h"

#include <algorithm>

namespace vrinject {
namespace vulkan {

VulkanGpuProfiler::VulkanGpuProfiler(double targetFrameMs)
    : m_targetFrameMs(targetFrameMs > 0.0 ? targetFrameMs : 11.111) {
}

VulkanGpuProfiler::~VulkanGpuProfiler() {
    Shutdown();
}

bool VulkanGpuProfiler::Initialize(VkDevice device, uint32_t queryCount) {
    m_device = device;
    if (!m_device || queryCount == 0) {
        m_snapshot.timestampQueriesSupported = false;
        return false;
    }

    VkQueryPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    info.queryType = VK_QUERY_TYPE_TIMESTAMP;
    info.queryCount = queryCount;

    if (vkCreateQueryPool(m_device, &info, nullptr, &m_queryPool) != VK_SUCCESS) {
        m_queryPool = VK_NULL_HANDLE;
        m_snapshot.timestampQueriesSupported = false;
        return false;
    }

    m_snapshot.timestampQueriesSupported = true;
    return true;
}

void VulkanGpuProfiler::Shutdown() {
    if (m_device && m_queryPool) {
        vkDestroyQueryPool(m_device, m_queryPool, nullptr);
    }
    m_queryPool = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
    m_snapshot.timestampQueriesSupported = false;
}

void VulkanGpuProfiler::RecordSample(const VulkanGpuProfilerSample& sample) {
    m_dispatchSamples[m_nextSample] = std::max(sample.dispatchMs, 0.0);
    m_copySamples[m_nextSample] = std::max(sample.copyMs, 0.0);
    m_barrierSamples[m_nextSample] = std::max(sample.barrierMs, 0.0);
    m_idleSamples[m_nextSample] = std::max(sample.queueIdleMs, 0.0);

    m_nextSample = (m_nextSample + 1) % WINDOW_SIZE;
    m_windowCount = std::min(m_windowCount + 1, WINDOW_SIZE);
    m_totalSamples++;
    RecomputeSnapshot();
}

void VulkanGpuProfiler::Reset() {
    m_nextSample = 0;
    m_windowCount = 0;
    m_totalSamples = 0;
    m_dispatchSamples.fill(0.0);
    m_copySamples.fill(0.0);
    m_barrierSamples.fill(0.0);
    m_idleSamples.fill(0.0);
    const bool timestampSupported = m_snapshot.timestampQueriesSupported;
    m_snapshot = {};
    m_snapshot.timestampQueriesSupported = timestampSupported;
}

double VulkanGpuProfiler::Average(const std::array<double, WINDOW_SIZE>& samples, size_t count) const {
    if (count == 0) return 0.0;

    double total = 0.0;
    for (size_t i = 0; i < count; ++i) {
        total += samples[i];
    }
    return total / static_cast<double>(count);
}

void VulkanGpuProfiler::RecomputeSnapshot() {
    const bool timestampSupported = m_snapshot.timestampQueriesSupported;
    m_snapshot.dispatchMs = Average(m_dispatchSamples, m_windowCount);
    m_snapshot.copyMs = Average(m_copySamples, m_windowCount);
    m_snapshot.barrierMs = Average(m_barrierSamples, m_windowCount);
    m_snapshot.queueIdleMs = Average(m_idleSamples, m_windowCount);
    const double busyMs = m_snapshot.dispatchMs + m_snapshot.copyMs + m_snapshot.barrierMs;
    m_snapshot.gpuUtilization = m_targetFrameMs > 0.0
        ? std::clamp(busyMs / m_targetFrameMs, 0.0, 1.0)
        : 0.0;
    m_snapshot.sampleCount = m_totalSamples;
    m_snapshot.timestampQueriesSupported = timestampSupported;
}

} // namespace vulkan
} // namespace vrinject
