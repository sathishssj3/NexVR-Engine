#pragma once

#include <cstdint>
#include <string>

namespace vrinject {
namespace ai {

struct TelemetryData {
    double gpuTimeMs = 0.0;
    double cpuTimeMs = 0.0;
    double queueWaitMs = 0.0;
    double fenceWaitMs = 0.0;
    uint32_t droppedFrames = 0;
    
    // Sprint 6.5 Expanded Telemetry
    uint32_t peakQueueDepth = 0;
    double inferenceThroughput = 0.0; // jobs/sec
    uint32_t fallbackFrequency = 0;
    double initTimeMs = 0.0;
};

struct MemoryUsage {
    uint64_t modelBytes = 0;
    uint64_t tensorBytes = 0;
    uint64_t temporaryBytes = 0;
    uint64_t totalBytes = 0;
};

class IAIBackend {
public:
    virtual ~IAIBackend() = default;

    // Lifecycle
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;

    // Resource Management
    virtual bool CreateResources() = 0;
    virtual void DestroyResources() = 0;

    // Inference Execution
    // Returns a backend-specific job/fence ID that can be polled
    virtual uint64_t SubmitInference(uint64_t frameId) = 0;

    // Non-blocking poll for job completion
    virtual bool PollCompletion(uint64_t jobId) = 0;

    // Blocking wait for job completion
    virtual void Synchronize(uint64_t jobId) = 0;

    // Telemetry & Reporting
    virtual MemoryUsage GetMemoryUsage() const = 0;
    virtual TelemetryData GetTelemetry() const = 0;
    
    // Backend Identifier
    virtual const char* GetName() const = 0;
};

} // namespace ai
} // namespace vrinject
