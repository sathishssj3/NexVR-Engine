#include "engine_detector.h"
#include "logger.h"

namespace vrinject {

void EngineDetector::Detect() {
    LOG_INFO("Detecting game engine...");

    // Check for Unity first (it's the quickest check — just module names)
    HMODULE il2cpp = GetModuleHandleA("GameAssembly.dll");
    HMODULE mono = GetModuleHandleA("mono-2.0-bdwgc.dll");
    if (!mono) mono = GetModuleHandleA("mono.dll");

    if (il2cpp || mono) {
        m_engineType = EngineType::Unity;
        m_versionString = il2cpp ? "Unity (IL2CPP)" : "Unity (Mono)";
        LOG_INFO("Detected %s engine.", m_versionString.c_str());
        return;
    }

    // Check for Unreal Engine
    if (CheckForUnrealWindow()) {
        LOG_INFO("Detected UnrealWindow class - Game is running Unreal Engine.");
        m_engineType = EngineType::UnrealEngine4; // Default to UE4, will refine via signature scanning
        
        // Scan for signatures to determine exact UE4/UE5 version
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
    // The detailed version detection is now handled by UnrealScanner::DetermineVersion().
    // Here we just set a basic version string.
    m_versionString = "UE4.xx";
    return true;
}

} // namespace vrinject
