#pragma once
#include <cstdint>
#include <atomic>
#include <chrono>
#include <thread>

namespace vrinject {

enum class DiagnosticLevel {
    Info,
    Warning,
    Error,
    Critical
};

enum class DiagnosticDeliveryPolicy {
    DROP_ALLOWED,
    TRACK_DROP,
    EMERGENCY_FALLBACK
};

struct DiagnosticQueueStats {
    uint64_t eventsEnqueued = 0;
    uint64_t eventsDropped = 0;
    uint64_t queueHighWatermark = 0;
    uint64_t enqueueFailures = 0;
    uint64_t workerWakeups = 0;
};

struct DiagnosticEvent {
    std::chrono::system_clock::time_point Timestamp;
    DiagnosticLevel Level;
    char Subsystem[32];
    char Message[128];
};

class DiagnosticContext {
public:
    // Must be strictly non-blocking. No locks, no dynamic allocations.
    void PostEvent(DiagnosticLevel level, const char* subsystem, const char* message);
    
    DiagnosticQueueStats GetStats() const;
    void StartWorker();
    void StopWorker();

    DiagnosticContext();
    ~DiagnosticContext();

private:
    void WorkerThread();

    static constexpr size_t QUEUE_SIZE = 1024;
    DiagnosticEvent m_queue[QUEUE_SIZE];
    
    // Very simple wait-free bounded MPSC mechanism using atomics
    std::atomic<size_t> m_head{0};
    std::atomic<size_t> m_tail{0};
    
    // Telemetry
    std::atomic<uint64_t> m_eventsEnqueued{0};
    std::atomic<uint64_t> m_eventsDropped{0};
    std::atomic<uint64_t> m_enqueueFailures{0};
    std::atomic<uint64_t> m_workerWakeups{0};
    
    std::atomic<bool> m_running{false};
    std::thread m_worker;
};

} // namespace vrinject
