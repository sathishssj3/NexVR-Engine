#pragma once

#include <cstdint>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <memory>

#include "backend/ai_backend.h"

namespace vrinject {
namespace ai {

struct AIJob {
    uint64_t frameId;
};

class AIScheduler {
public:
    AIScheduler(std::unique_ptr<IAIBackend> backend);
    ~AIScheduler();

    // Push an inference job to the async queue
    void PushJob(uint64_t frameId);
    
    // Polls whether the AI job for the given frame is completed (zero-blocking)
    // If this returns false, the render thread MUST enter the DEGRADED Skip Enhancement state.
    bool IsJobReady(uint64_t frameId) const;

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
};

} // namespace ai
} // namespace vrinject
