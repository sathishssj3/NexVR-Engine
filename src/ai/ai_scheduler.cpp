#include "ai/ai_scheduler.h"
#include <iostream>

namespace vrinject {
namespace ai {

AIScheduler::AIScheduler(std::unique_ptr<IAIBackend> backend)
    : m_backend(std::move(backend)) {
    if (m_backend) {
        m_backend->Initialize();
        m_backend->CreateResources();
    }
    m_workerThread = std::thread(&AIScheduler::WorkerThreadLoop, this);
}

AIScheduler::~AIScheduler() {
    m_exit.store(true);
    m_cv.notify_one();
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
    if (m_backend) {
        m_backend->DestroyResources();
        m_backend->Shutdown();
    }
}

void AIScheduler::PushJob(const AIJob& job) {
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        
        // Task 2: Frame Queue Overflow Policy (Drop-Oldest)
        const size_t MAX_QUEUE_SIZE = 2;
        while (m_jobQueue.size() >= MAX_QUEUE_SIZE) {
            m_jobQueue.pop(); // Drop oldest
        }
        
        m_jobQueue.push(job);
    }
    m_cv.notify_one();
}

bool AIScheduler::IsJobReady(uint64_t frameId) const {
    // Zero-blocking poll.
    return m_completedFrameId.load(std::memory_order_acquire) >= frameId;
}

void AIScheduler::WorkerThreadLoop() {
    while (!m_exit.load()) {
        AIJob job;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_cv.wait(lock, [this]() { return m_exit.load() || !m_jobQueue.empty(); });
            
            if (m_exit.load() && m_jobQueue.empty()) {
                break;
            }
            
            job = m_jobQueue.front();
            m_jobQueue.pop();
        }
        
        if (m_backend) {
            uint64_t jobId = m_backend->SubmitInference(job);
            m_backend->Synchronize(jobId);
            m_latestUIMask.store(m_backend->GetUIMask(), std::memory_order_release);
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

} // namespace ai
} // namespace vrinject
