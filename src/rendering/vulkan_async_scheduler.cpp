#include "vulkan_async_scheduler.h"

namespace vrinject {
namespace vulkan {

VulkanAsyncSchedulerConfig VulkanAsyncScheduler::SelectQueueFamily(
    const std::vector<VkQueueFamilyProperties>& families,
    uint32_t graphicsQueueFamily) {
    VulkanAsyncSchedulerConfig config{};
    config.graphicsQueueFamily = graphicsQueueFamily;
    config.selectedQueueFamily = graphicsQueueFamily;

    for (uint32_t i = 0; i < families.size(); ++i) {
        const VkQueueFlags flags = families[i].queueFlags;
        const bool compute = (flags & VK_QUEUE_COMPUTE_BIT) != 0;
        const bool graphics = (flags & VK_QUEUE_GRAPHICS_BIT) != 0;
        if (compute && !graphics) {
            config.selectedQueueFamily = i;
            config.asyncComputeAvailable = true;
            return config;
        }
    }

    return config;
}

AICommandQueue::AICommandQueue() {
    m_workerThread = std::thread(&AICommandQueue::WorkerThreadLoop, this);
}

AICommandQueue::~AICommandQueue() {
    m_exit.store(true);
    m_cv.notify_one();
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

void AICommandQueue::PushJob(uint64_t frameId, std::function<void()> payload) {
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        
        // Task 2: Frame Queue Overflow Policy (Drop-Oldest)
        // Keep the queue extremely shallow to minimize latency. 
        // If the AI is falling behind, drop stale frames so it processes the freshest data.
        const size_t MAX_QUEUE_SIZE = 2;
        while (m_jobQueue.size() >= MAX_QUEUE_SIZE) {
            m_jobQueue.pop(); // Drop oldest
        }
        
        m_jobQueue.push({frameId, std::move(payload)});
    }
    m_cv.notify_one();
}

bool AICommandQueue::IsJobReady(uint64_t frameId) const {
    // Zero-blocking poll.
    return m_completedFrameId.load(std::memory_order_acquire) >= frameId;
}

void AICommandQueue::WorkerThreadLoop() {
    while (!m_exit.load()) {
        AIJob job;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_cv.wait(lock, [this]() { return m_exit.load() || !m_jobQueue.empty(); });
            
            if (m_exit.load() && m_jobQueue.empty()) {
                break;
            }
            
            job = std::move(m_jobQueue.front());
            m_jobQueue.pop();
        }
        
        if (job.payload) {
            job.payload();
        }
        
        // Mark this frame (and any older skipped frames) as completed.
        uint64_t currentCompleted = m_completedFrameId.load(std::memory_order_relaxed);
        while (currentCompleted < job.frameId) {
            if (m_completedFrameId.compare_exchange_weak(currentCompleted, job.frameId, std::memory_order_release, std::memory_order_relaxed)) {
                break;
            }
        }
    }
}

} // namespace vulkan
} // namespace vrinject
