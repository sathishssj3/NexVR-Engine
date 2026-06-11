#include "engine_detector.h"
#include "logger.h"

namespace vrinject {

void EngineDetector::Detect() {
    LOG_INFO("Detecting game engine...");

    if (CheckForUnrealWindow()) {
        LOG_INFO("Detected UnrealWindow class - Game is running Unreal Engine.");
        m_engineType = EngineType::UnrealEngine4; // Default to UE4 for now, will refine via signature scanning
        
        // Next: Scan for signatures to determine exact UE4/UE5 version
        ScanForUnrealSignatures();
        return;
    }

    LOG_WARN("Engine type unknown. Defaulting to generic hooks.");
}

bool EngineDetector::CheckForUnrealWindow() {
    // A quick heuristic: Check if the main window class is "UnrealWindow"
    HWND hwnd = FindWindowA("UnrealWindow", nullptr);
    if (hwnd) {
        return true;
    }
    
    // Sometimes games rename it, so we enumerate windows belonging to our process
    DWORD currentPid = GetCurrentProcessId();
    bool foundUnreal = false;
    
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid == *reinterpret_cast<DWORD*>(lParam)) {
            char className[256];
            if (GetClassNameA(hwnd, className, sizeof(className))) {
                if (std::string(className).find("UnrealWindow") != std::string::npos) {
                    *reinterpret_cast<bool*>(lParam + sizeof(DWORD)) = true;
                    return FALSE; // Stop enumerating
                }
            }
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&currentPid));
    
    return foundUnreal;
}

bool EngineDetector::ScanForUnrealSignatures() {
    // TODO: Implement actual memory scanning to find GEngine and determine version
    // For now, we stub it out.
    m_versionString = "UE4.xx";
    return true;
}

} // namespace vrinject
