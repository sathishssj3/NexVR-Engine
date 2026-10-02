#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <string>
#include <vector>

namespace vrinject {

enum class EngineType {
    Unknown,
    UnrealEngine4,
    UnrealEngine5,
    Unity
};

struct EngineTuningProfile {
    bool reverseZ = true;
    bool rowMajorMatrices = false;
    bool excludeUiPasses = true;
    bool excludeShadowPasses = true;
};

struct EngineDetection {
    EngineType type = EngineType::Unknown;
    std::string versionString = "Unknown";
    float confidence = 0.0f;
    EngineTuningProfile tuning{};
};

class EngineDetector {
public:
    EngineDetector() = default;

    void Detect();
    EngineType GetEngineType() const { return m_engineType; }
    const std::string& GetEngineVersionString() const { return m_versionString; }
    const EngineDetection& GetDetection() const { return m_detection; }

    static EngineDetection DetectFromModuleNames(const std::vector<std::string>& moduleNames);

private:
    
    bool CheckForUnrealWindow();
    bool ScanForUnrealSignatures();
    std::vector<std::string> EnumerateCurrentProcessModules() const;
    void ApplyDetection(const EngineDetection& detection);

    EngineType m_engineType = EngineType::Unknown;
    std::string m_versionString = "Unknown";
    EngineDetection m_detection{};
};

} // namespace vrinject
