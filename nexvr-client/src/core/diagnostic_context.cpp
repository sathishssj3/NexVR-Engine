#include "core/diagnostic_context.h"
#include <cstring>
#include <iostream>
#include "core/logger.h"

#ifdef _WIN32
#define strncpy_safe(dest, src, size) strncpy_s(dest, size, src, _TRUNCATE)
#else
#define strncpy_safe(dest, src, size) strncpy(dest, src, size - 1); dest[size - 1] = '\0'
#endif

namespace vrinject {

DiagnosticContext::DiagnosticContext() {
    StartWorker();
}

DiagnosticContext::~DiagnosticContext() {
    StopWorker();
}

void DiagnosticContext::StartWorker() {
    if (!m_running.exchange(true)) {
        m_worker = std::thread(&DiagnosticContext::WorkerThread, this);
    }
}

void DiagnosticContext::StopWorker() {
    if (m_running.exchange(false)) {
        if (m_worker.joinable()) {
            m_worker.join();
        }
    }
}

void DiagnosticContext::PostEvent(DiagnosticLevel level, const char* subsystem, const char* message) {
    // Read current head
    size_t head = m_head.load(std::memory_order_relaxed);
    size_t next_head = (head + 1) % QUEUE_SIZE;
    
    // Check if full
    if (next_head == m_tail.load(std::memory_order_acquire)) {
        // Queue full
        m_eventsDropped.fetch_add(1, std::memory_order_relaxed);
        
        if (level == DiagnosticLevel::Critical) {
            // Drop allowed but increment critical-loss counter (handled by level-checking logic in worker if possible, or just raw failure count here)
            m_enqueueFailures.fetch_add(1, std::memory_order_relaxed);
        }
        return;
    }

    // Attempt to reserve space via CAS
    while (!m_head.compare_exchange_weak(head, next_head, std::memory_order_acquire, std::memory_order_relaxed)) {
        next_head = (head + 1) % QUEUE_SIZE;
        if (next_head == m_tail.load(std::memory_order_acquire)) {
            m_eventsDropped.fetch_add(1, std::memory_order_relaxed);
            if (level == DiagnosticLevel::Critical) m_enqueueFailures.fetch_add(1, std::memory_order_relaxed);
            return;
        }
    }

    // Space reserved at 'head', fill data
    auto& evt = m_queue[head];
    evt.Timestamp = std::chrono::system_clock::now();
    evt.Level = level;
    strncpy_safe(evt.Subsystem, subsystem, sizeof(evt.Subsystem));
    strncpy_safe(evt.Message, message, sizeof(evt.Message));
    
    m_eventsEnqueued.fetch_add(1, std::memory_order_relaxed);
}

DiagnosticQueueStats DiagnosticContext::GetStats() const {
    DiagnosticQueueStats stats;
    stats.eventsEnqueued = m_eventsEnqueued.load(std::memory_order_relaxed);
    stats.eventsDropped = m_eventsDropped.load(std::memory_order_relaxed);
    stats.enqueueFailures = m_enqueueFailures.load(std::memory_order_relaxed);
    stats.workerWakeups = m_workerWakeups.load(std::memory_order_relaxed);
    
    // Calculate current size
    size_t head = m_head.load(std::memory_order_relaxed);
    size_t tail = m_tail.load(std::memory_order_relaxed);
    size_t currentSize = (head >= tail) ? (head - tail) : (QUEUE_SIZE - tail + head);
    stats.queueHighWatermark = currentSize; // Approximation for now
    
    return stats;
}

void DiagnosticContext::WorkerThread() {
    while (m_running.load(std::memory_order_relaxed)) {
        size_t tail = m_tail.load(std::memory_order_relaxed);
        size_t head = m_head.load(std::memory_order_acquire);
        
        if (tail != head) {
            m_workerWakeups.fetch_add(1, std::memory_order_relaxed);
            while (tail != head) {
                // Process event at 'tail'
                auto& evt = m_queue[tail];
                
                // Formatted output to stdout (in real system to ETW or file)
                // Just use iostream for prototype, avoid locking
                const char* lvlStr = "INFO";
                if (evt.Level == DiagnosticLevel::Warning) lvlStr = "WARN";
                else if (evt.Level == DiagnosticLevel::Error) lvlStr = "ERROR";
                else if (evt.Level == DiagnosticLevel::Critical) lvlStr = "FATAL";
                
                // Do actual logging logic here without holding up producer
                // LOG_INFO, etc can be used if thread-safe
                
                tail = (tail + 1) % QUEUE_SIZE;
            }
            m_tail.store(tail, std::memory_order_release);
        } else {
            // Sleep briefly to yield CPU, a real system might use Event/ConditionVariable, 
            // but for minimal blocking a small sleep is fine.
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

} // namespace vrinject
