#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <string>

namespace vrinject {

enum class EngineType {
    Unknown,
    UnrealEngine4,
    UnrealEngine5,
    Unity
};

class EngineDetector {
public:
    static EngineDetector& Get() {
        static EngineDetector instance;
        return instance;
    }

    void Detect();
    EngineType GetEngineType() const { return m_engineType; }
    const std::string& GetEngineVersionString() const { return m_versionString; }

private:
    EngineDetector() = default;
    
    bool CheckForUnrealWindow();
    bool ScanForUnrealSignatures();

    EngineType m_engineType = EngineType::Unknown;
    std::string m_versionString = "Unknown";
};

} // namespace vrinject
