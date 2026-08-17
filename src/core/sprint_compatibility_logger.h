#pragma once
#include <string>
#include <cstdint>
#include <mutex>
#include <fstream>

namespace vrinject {

struct SprintCompatibilityScore {
    std::string Title;
    std::string Engine; // Detected engine string e.g. "UE4", "Unity"
    float EngineDetectionConfidence; // 0.0 to 1.0
    
    bool InjectionSuccess;
    
    bool CameraLockSuccess;
    double CameraTimeToLockMs;
    
    bool DepthLockSuccess;
    double DepthTimeToLockMs;
    
    double PredictedDisplayTimingDeltaMs; // From OpenXRHealthMonitor
    double ActualGpuTimeMs; // Stereo render cost
    double ActualCpuTimeMs; // Stereo CPU cost
    
    std::string FailureReason; // Crash dump identifier or descriptive string
};

class SprintCompatibilityLogger {
public:
    // Initialize file (write JSON array start)
    void Initialize(const std::string& logFilePath = "sprint59_compatibility_matrix.json");
    
    // Write a structured entry
    void LogScore(const SprintCompatibilityScore& score);
    
    // Finalize file (close JSON array)
    void Shutdown();

    SprintCompatibilityLogger();
    ~SprintCompatibilityLogger();

private:
    std::mutex m_mutex;
    std::ofstream m_file;
    bool m_isFirstEntry;
    bool m_initialized;
};

} // namespace vrinject
