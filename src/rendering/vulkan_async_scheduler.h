#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <functional>
#include <atomic>

namespace vrinject {
namespace vulkan {

struct VulkanAsyncSchedulerConfig {
    uint32_t graphicsQueueFamily = 0;
    uint32_t selectedQueueFamily = 0;
    bool asyncComputeAvailable = false;
};

struct AIJob {
    uint64_t frameId;
    std::function<void()> payload;
};

class VulkanAsyncScheduler {
public:
    static VulkanAsyncSchedulerConfig SelectQueueFamily(
        const std::vector<VkQueueFamilyProperties>& families,
        uint32_t graphicsQueueFamily);
};

class AICommandQueue {
public:
    AICommandQueue();
    ~AICommandQueue();

    // Push an inference job to the async queue
    void PushJob(uint64_t frameId, std::function<void()> payload);
    
    // Polls whether the AI job for the given frame is completed (zero-blocking)
    // If this returns false, the render thread MUST enter the DEGRADED Skip Enhancement state.
    bool IsJobReady(uint64_t frameId) const;

private:
    void WorkerThreadLoop();

    std::queue<AIJob> m_jobQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_cv;
    std::thread m_workerThread;
    std::atomic<bool> m_exit{false};
    std::atomic<uint64_t> m_completedFrameId{0};
};

} // namespace vulkan
} // namespace vrinject
