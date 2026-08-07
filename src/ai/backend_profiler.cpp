#include "backend_profiler.h"
#include <fstream>
#include <iostream>

namespace vrinject {
namespace ai {

void BackendProfiler::RecordTelemetry(const std::string& backendName, const TelemetryData& data, const MemoryUsage& memory) {
    m_entries.push_back({backendName, data, memory});
}

bool BackendProfiler::ExportBackendComparison(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) return false;
    file << "Backend,InitTime(ms),DroppedFrames,FallbackFreq,PeakQueue\n";
    for (const auto& entry : m_entries) {
        file << entry.backendName << ","
             << entry.telemetry.initTimeMs << ","
             << entry.telemetry.droppedFrames << ","
             << entry.telemetry.fallbackFrequency << ","
             << entry.telemetry.peakQueueDepth << "\n";
    }
    return true;
}

bool BackendProfiler::ExportLatencyTrace(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) return false;
    file << "Backend,GPUTime(ms),CPUTime(ms),QueueWait(ms),FenceWait(ms)\n";
    for (const auto& entry : m_entries) {
        file << entry.backendName << ","
             << entry.telemetry.gpuTimeMs << ","
             << entry.telemetry.cpuTimeMs << ","
             << entry.telemetry.queueWaitMs << ","
             << entry.telemetry.fenceWaitMs << "\n";
    }
    return true;
}

bool BackendProfiler::ExportSchedulerMetrics(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) return false;
    file << "Backend,InferenceThroughput(jobs/s)\n";
    for (const auto& entry : m_entries) {
        file << entry.backendName << ","
             << entry.telemetry.inferenceThroughput << "\n";
    }
    return true;
}

bool BackendProfiler::ExportMemoryReport(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) return false;
    file << "Backend,ModelBytes,TensorBytes,TemporaryBytes,TotalBytes\n";
    for (const auto& entry : m_entries) {
        file << entry.backendName << ","
             << entry.memory.modelBytes << ","
             << entry.memory.tensorBytes << ","
             << entry.memory.temporaryBytes << ","
             << entry.memory.totalBytes << "\n";
    }
    return true;
}

} // namespace ai
} // namespace vrinject
