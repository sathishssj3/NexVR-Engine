#include "backend_profiler.h"
#include <fstream>
#include <iostream>

namespace vrinject {
namespace ai {

void BackendProfiler::RecordTelemetry(const std::string& backendName, const TelemetryData& data, const MemoryUsage& memory) {
    m_entries.push_back({backendName, data, memory});
}

bool BackendProfiler::ExportToCSV(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) return false;
    
    file << "Backend,GPU Time (ms),CPU Time (ms),Queue Wait (ms),Fence Wait (ms),Dropped Frames,Total Memory (B)\n";
    
    for (const auto& entry : m_entries) {
        file << entry.backendName << ","
             << entry.telemetry.gpuTimeMs << ","
             << entry.telemetry.cpuTimeMs << ","
             << entry.telemetry.queueWaitMs << ","
             << entry.telemetry.fenceWaitMs << ","
             << entry.telemetry.droppedFrames << ","
             << entry.memory.totalBytes << "\n";
    }
    
    return true;
}

} // namespace ai
} // namespace vrinject
