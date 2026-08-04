#include "sprint_compatibility_logger.h"
#include <iostream>

namespace vrinject {

SprintCompatibilityLogger::SprintCompatibilityLogger() 
    : m_isFirstEntry(true), m_initialized(false) {
}

SprintCompatibilityLogger::~SprintCompatibilityLogger() {
    Shutdown();
}

void SprintCompatibilityLogger::Initialize(const std::string& logFilePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) return;
    
    m_file.open(logFilePath, std::ios::out | std::ios::trunc);
    if (m_file.is_open()) {
        m_file << "[\n"; // Start of JSON array
        m_initialized = true;
    } else {
        std::cerr << "[SprintCompatibilityLogger] Failed to open log file: " << logFilePath << std::endl;
    }
}

void SprintCompatibilityLogger::LogScore(const CompatibilityScore& score) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return;

    if (!m_isFirstEntry) {
        m_file << ",\n";
    }
    m_isFirstEntry = false;

    // Helper lambda to escape strings for JSON
    auto escapeJson = [](const std::string& s) -> std::string {
        std::string res;
        for (char c : s) {
            if (c == '"') res += "\\\"";
            else if (c == '\\') res += "\\\\";
            else if (c == '\b') res += "\\b";
            else if (c == '\f') res += "\\f";
            else if (c == '\n') res += "\\n";
            else if (c == '\r') res += "\\r";
            else if (c == '\t') res += "\\t";
            else res += c;
        }
        return res;
    };

    m_file << "  {\n"
           << "    \"title\": \"" << escapeJson(score.Title) << "\",\n"
           << "    \"engine\": \"" << escapeJson(score.Engine) << "\",\n"
           << "    \"engine_detection_confidence\": " << score.EngineDetectionConfidence << ",\n"
           << "    \"injection_success\": " << (score.InjectionSuccess ? "true" : "false") << ",\n"
           << "    \"camera_lock_success\": " << (score.CameraLockSuccess ? "true" : "false") << ",\n"
           << "    \"camera_time_to_lock_ms\": " << score.CameraTimeToLockMs << ",\n"
           << "    \"depth_lock_success\": " << (score.DepthLockSuccess ? "true" : "false") << ",\n"
           << "    \"depth_time_to_lock_ms\": " << score.DepthTimeToLockMs << ",\n"
           << "    \"predicted_display_timing_delta_ms\": " << score.PredictedDisplayTimingDeltaMs << ",\n"
           << "    \"actual_gpu_time_ms\": " << score.ActualGpuTimeMs << ",\n"
           << "    \"actual_cpu_time_ms\": " << score.ActualCpuTimeMs << ",\n"
           << "    \"failure_reason\": \"" << escapeJson(score.FailureReason) << "\"\n"
           << "  }";
           
    m_file.flush();
}

void SprintCompatibilityLogger::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized && m_file.is_open()) {
        m_file << "\n]\n"; // End of JSON array
        m_file.close();
        m_initialized = false;
    }
}

} // namespace vrinject
