#pragma once

#include "backend/ai_backend.h"
#include <string>
#include <vector>

namespace vrinject {
namespace ai {

class BackendProfiler {
public:
    void RecordTelemetry(const std::string& backendName, const TelemetryData& data, const MemoryUsage& memory);
    
    // Export recorded data to CSVs
    bool ExportBackendComparison(const std::string& filepath) const;
    bool ExportLatencyTrace(const std::string& filepath) const;
    bool ExportSchedulerMetrics(const std::string& filepath) const;
    bool ExportMemoryReport(const std::string& filepath) const;

private:
    struct Entry {
        std::string backendName;
        TelemetryData telemetry;
        MemoryUsage memory;
    };
    std::vector<Entry> m_entries;
};

} // namespace ai
} // namespace vrinject
