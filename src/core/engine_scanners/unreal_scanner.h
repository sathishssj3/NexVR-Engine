#pragma once

#include <windows.h>
#include <string>
#include <cstdint>

namespace vrinject {
namespace engine_scanners {

// Detected Unreal Engine sub-version for signature selection
enum class UEVersion {
    Unknown,
    UE4_25,     // 4.25 - 4.27
    UE5_0,      // 5.0 - 5.1
    UE5_2,      // 5.2 - 5.4+
};

class UnrealScanner {
public:
    static UnrealScanner& Get() {
        static UnrealScanner instance;
        return instance;
    }

    bool Initialize();
    void Shutdown();

    bool IsUnrealEngine() const { return m_isUnreal; }
    UEVersion GetVersion() const { return m_version; }

    // Returns true if successfully hooked the camera pipeline
    bool HookCamera();

    // Accessors for resolved pointers (used by UnrealHook)
    void** GetGWorldPtr() const { return m_gWorldPtr; }
    void** GetGEnginePtr() const { return m_gEnginePtr; }
    void*  GetProjectionDataFunc() const { return m_getProjectionDataFunc; }
    void*  GetUpdateCameraFunc() const { return m_updateCameraFunc; }

private:
    UnrealScanner() = default;
    ~UnrealScanner() = default;

    // Attempt to find GWorld using multiple known signatures
    bool ScanForGWorld();
    // Attempt to find GEngine using multiple known signatures
    bool ScanForGEngine();
    // Attempt to find GetProjectionData/CalcSceneView
    bool ScanForProjectionFunction();
    // Determine UE4 vs UE5 based on which signatures matched
    void DetermineVersion();

    bool m_isUnreal = false;
    bool m_initialized = false;
    UEVersion m_version = UEVersion::Unknown;

    void** m_gWorldPtr = nullptr;         // Resolved pointer to UWorld*
    void** m_gEnginePtr = nullptr;        // Resolved pointer to GEngine*
    void*  m_getProjectionDataFunc = nullptr;  // Address of GetProjectionData/CalcSceneView
    void*  m_updateCameraFunc = nullptr;       // Address of APlayerCameraManager::UpdateCamera
};

} // namespace engine_scanners
} // namespace vrinject
