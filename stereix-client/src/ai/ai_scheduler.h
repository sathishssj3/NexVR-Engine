#pragma once

#include <cstdint>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <memory>

#include "ai/backend/ai_backend.h"

namespace vrinject {
namespace ai {


class AIScheduler {
public:
    AIScheduler(std::unique_ptr<IAIBackend> backend);
    ~AIScheduler();

    // Push an inference job to the async queue
    void PushJob(const AIJob& job);
    
    // Polls whether the AI job for the given frame is completed (zero-blocking)
    // If this returns false, the render thread MUST enter the DEGRADED Skip Enhancement state.
    bool IsJobReady(uint64_t frameId) const;

    // Retrieve the latest available UI segmentation mask texture pointer (backend-specific)
    void* GetLatestUIMask() const { return m_latestUIMask.load(std::memory_order_relaxed); }

    IAIBackend* GetBackend() const { return m_backend.get(); }

private:
    void WorkerThreadLoop();

    std::unique_ptr<IAIBackend> m_backend;

    std::queue<AIJob> m_jobQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_cv;
    std::thread m_workerThread;
    std::atomic<bool> m_exit{false};
    std::atomic<uint64_t> m_completedFrameId{0};
    std::atomic<void*> m_latestUIMask{nullptr};
};

} // namespace ai
} // namespace vrinject
