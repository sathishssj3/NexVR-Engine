#include "unreal_scanner.h"
#include "../logger.h"
#include "../memory_scanner.h"
#include "../ue_sdk.h"

namespace vrinject {
namespace engine_scanners {

bool UnrealScanner::Initialize() {
    if (m_initialized) return true;

    LOG_INFO("UnrealScanner: Initializing memory scanner...");
    if (!MemoryScanner::Get().Initialize()) {
        LOG_ERROR("UnrealScanner: MemoryScanner failed to initialize.");
        return false;
    }

    // Step 1: Scan for GWorld — this is the primary indicator that we're in Unreal
    bool foundGWorld = ScanForGWorld();
    
    // Step 2: Scan for GEngine — useful for version detection and viewport access
    bool foundGEngine = ScanForGEngine();

    if (foundGWorld || foundGEngine) {
        m_isUnreal = true;
        DetermineVersion();
        LOG_INFO("UnrealScanner: Confirmed Unreal Engine! Version: %s",
            m_version == UEVersion::UE4_25 ? "UE4.25-4.27" :
            m_version == UEVersion::UE5_0  ? "UE5.0-5.1" :
            m_version == UEVersion::UE5_2  ? "UE5.2+" : "Unknown");
    } else {
        LOG_INFO("UnrealScanner: GWorld/GEngine not found. Not an Unreal Engine game, or unsupported version.");
    }

    // Step 3: Scan for the projection/camera function we need to hook
    if (m_isUnreal) {
        ScanForProjectionFunction();
    }

    m_initialized = true;
    return true;
}

void UnrealScanner::Shutdown() {
    m_initialized = false;
    m_isUnreal = false;
    m_version = UEVersion::Unknown;
    m_gWorldPtr = nullptr;
    m_getProjectionDataFunc = nullptr;
    m_updateCameraFunc = nullptr;
    m_beginRenderingViewFamilyFunc = nullptr;
}

bool UnrealScanner::ScanForGWorld() {
    auto& scanner = MemoryScanner::Get();

    // Try UE4 pattern first (most common)
    LOG_INFO("UnrealScanner: Scanning for GWorld (UE4 pattern)...");
    uint8_t* result = scanner.ScanSignature(ue::Signatures::GWorld_UE4);
    if (result) {
        // The instruction is: mov rbx, [rip+OFFSET]
        // Instruction is 7 bytes: 48 8B 1D [4-byte offset]
        // RIP offset starts at byte 3, instruction length is 7
        m_gWorldPtr = reinterpret_cast<void**>(scanner.ResolveRIP(result, 7, 3));
        if (m_gWorldPtr && *m_gWorldPtr) {
            LOG_INFO("UnrealScanner: Found GWorld (UE4) at %p -> %p", m_gWorldPtr, *m_gWorldPtr);
            return true;
        }
        // Resolved but points to null — game may not have loaded the world yet
        if (m_gWorldPtr) {
            LOG_WARN("UnrealScanner: Found GWorld pointer at %p but world is null (not yet loaded). Will retry later.", m_gWorldPtr);
            return true;
        }
    }

    // Try UE5 pattern
    LOG_INFO("UnrealScanner: Scanning for GWorld (UE5 pattern)...");
    result = scanner.ScanSignature(ue::Signatures::GWorld_UE5);
    if (result) {
        // mov rax, [rip+OFFSET] — 7 bytes, offset at byte 3
        m_gWorldPtr = reinterpret_cast<void**>(scanner.ResolveRIP(result, 7, 3));
        if (m_gWorldPtr) {
            LOG_INFO("UnrealScanner: Found GWorld (UE5) at %p -> %p", m_gWorldPtr, 
                     *m_gWorldPtr ? *m_gWorldPtr : nullptr);
            return true;
        }
    }

    // Try UE5 alternate pattern
    LOG_INFO("UnrealScanner: Scanning for GWorld (UE5 Alt pattern)...");
    result = scanner.ScanSignature(ue::Signatures::GWorld_UE5_Alt);
    if (result) {
        m_gWorldPtr = reinterpret_cast<void**>(scanner.ResolveRIP(result, 7, 3));
        if (m_gWorldPtr) {
            LOG_INFO("UnrealScanner: Found GWorld (UE5 Alt) at %p -> %p", m_gWorldPtr,
                     *m_gWorldPtr ? *m_gWorldPtr : nullptr);
            return true;
        }
    }

    LOG_WARN("UnrealScanner: Could not find GWorld with any known signature.");
    return false;
}

bool UnrealScanner::ScanForGEngine() {
    auto& scanner = MemoryScanner::Get();

    LOG_INFO("UnrealScanner: Scanning for GEngine (UE4 pattern)...");
    uint8_t* result = scanner.ScanSignature(ue::Signatures::GEngine_UE4);
    if (result) {
        // mov rcx, [rip+OFFSET] — 7 bytes, offset at byte 3
        m_gEnginePtr = reinterpret_cast<void**>(scanner.ResolveRIP(result, 7, 3));
        if (m_gEnginePtr) {
            LOG_INFO("UnrealScanner: Found GEngine (UE4) at %p -> %p", m_gEnginePtr,
                     *m_gEnginePtr ? *m_gEnginePtr : nullptr);
            return true;
        }
    }

    LOG_INFO("UnrealScanner: Scanning for GEngine (UE5 pattern)...");
    result = scanner.ScanSignature(ue::Signatures::GEngine_UE5);
    if (result) {
        m_gEnginePtr = reinterpret_cast<void**>(scanner.ResolveRIP(result, 7, 3));
        if (m_gEnginePtr) {
            LOG_INFO("UnrealScanner: Found GEngine (UE5) at %p -> %p", m_gEnginePtr,
                     *m_gEnginePtr ? *m_gEnginePtr : nullptr);
            return true;
        }
    }

    LOG_WARN("UnrealScanner: Could not find GEngine with any known signature.");
    return false;
}

bool UnrealScanner::ScanForProjectionFunction() {
    auto& scanner = MemoryScanner::Get();

    // Try GetProjectionData (UE4) first
    LOG_INFO("UnrealScanner: Scanning for GetProjectionData (UE4)...");
    uint8_t* result = scanner.ScanSignature(ue::Signatures::GetProjectionData_UE4);
    if (result) {
        m_getProjectionDataFunc = result;
        LOG_INFO("UnrealScanner: Found GetProjectionData at %p", result);
        return true;
    }

    // Try CalcSceneView (UE5, replacement for GetProjectionData)
    LOG_INFO("UnrealScanner: Scanning for CalcSceneView (UE5)...");
    result = scanner.ScanSignature(ue::Signatures::CalcSceneView_UE5);
    if (result) {
        m_getProjectionDataFunc = result;
        LOG_INFO("UnrealScanner: Found CalcSceneView (UE5) at %p", result);
        return true;
    }

    // Try CalcSceneView alternate (UE5.1+)
    LOG_INFO("UnrealScanner: Scanning for CalcSceneView (UE5 Alt)...");
    result = scanner.ScanSignature(ue::Signatures::CalcSceneView_UE5_Alt);
    if (result) {
        m_getProjectionDataFunc = result;
        LOG_INFO("UnrealScanner: Found CalcSceneView (UE5 Alt) at %p", result);
        return true;
    }

    // Try UpdateCamera as a fallback hook point
    LOG_INFO("UnrealScanner: Scanning for UpdateCamera (UE4)...");
    result = scanner.ScanSignature(ue::Signatures::UpdateCamera_UE4);
    if (result) {
        m_updateCameraFunc = result;
        LOG_INFO("UnrealScanner: Found UpdateCamera (UE4) at %p", result);
        return true;
    }

    LOG_INFO("UnrealScanner: Scanning for UpdateCamera (UE5)...");
    result = scanner.ScanSignature(ue::Signatures::UpdateCamera_UE5);
    if (result) {
        m_updateCameraFunc = result;
        LOG_INFO("UnrealScanner: Found UpdateCamera (UE5) at %p", result);
        return true;
    }

    LOG_WARN("UnrealScanner: Could not find any camera function to hook. "
             "Will rely on Universal Mode (depth reprojection).");

    // Also scan for BeginRenderingViewFamily
    LOG_INFO("UnrealScanner: Scanning for BeginRenderingViewFamily (UE4)...");
    result = scanner.ScanSignature(ue::Signatures::BeginRenderingViewFamily_UE4);
    if (result) {
        m_beginRenderingViewFamilyFunc = result;
        LOG_INFO("UnrealScanner: Found BeginRenderingViewFamily at %p", result);
        return true;
    }

    return false;
}

void UnrealScanner::DetermineVersion() {
    // Use which GWorld signature matched as the primary version indicator
    auto& scanner = MemoryScanner::Get();
    
    // If the UE4 GWorld signature was the one that resolved, it's UE4
    // We re-scan quickly to determine which pattern matched
    if (scanner.ScanSignature(ue::Signatures::GWorld_UE4)) {
        m_version = UEVersion::UE4_25;
    } else if (scanner.ScanSignature(ue::Signatures::GWorld_UE5) || 
               scanner.ScanSignature(ue::Signatures::GWorld_UE5_Alt)) {
        // Differentiate UE5.0 vs UE5.2+ by checking CalcSceneView signatures
        if (scanner.ScanSignature(ue::Signatures::CalcSceneView_UE5_Alt)) {
            m_version = UEVersion::UE5_2;
        } else {
            m_version = UEVersion::UE5_0;
        }
    }
}

bool UnrealScanner::HookCamera() {
    if (!m_initialized || !m_isUnreal) {
        LOG_WARN("UnrealScanner::HookCamera called but scanner not ready.");
        return false;
    }

    if (!m_getProjectionDataFunc && !m_updateCameraFunc) {
        LOG_WARN("UnrealScanner: No camera function found to hook. Falling back to Universal Mode.");
        return false;
    }

    // The actual MinHook detour installation is done by UnrealHook,
    // which reads the function address from us.
    LOG_INFO("UnrealScanner: Camera function located. Ready for UnrealHook to install detour.");
    return true;
}

} // namespace engine_scanners
} // namespace vrinject
