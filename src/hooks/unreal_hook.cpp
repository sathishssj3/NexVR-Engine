#include "unreal_hook.h"
#include "../core/logger.h"

#include "../core/memory_scanner.h"
#include "MinHook.h"

namespace vrinject {
namespace ue {

// Static Detour implementation
bool UnrealHook::DetourGetProjectionData(void* pLocalPlayer, void* pViewport, int stereoPass, void* pProjectionData) {
    // 1. Let the original function run first to populate pProjectionData with the engine's default camera math.
    bool result = Get().m_originalGetProjectionData(pLocalPlayer, pViewport, stereoPass, pProjectionData);

    // 2. Here we would overwrite the Location/Rotation/Matrix inside pProjectionData
    //    using our OpenXR pose data from the HMD!
    // Example:
    // FMinimalViewInfo* ViewInfo = (FMinimalViewInfo*)((uint8_t*)pProjectionData + offset_to_viewinfo);
    // ViewInfo->Rotation = OpenXRManager::Get().GetHeadRotationRotator();
    // ViewInfo->Location += OpenXRManager::Get().GetHeadPositionDelta();

    return result;
}

bool UnrealHook::Initialize() {
    LOG_INFO("UnrealHook::Initialize() - Attempting to hook Unreal Engine camera functions...");
    
    if (!MemoryScanner::Get().Initialize()) {
        LOG_ERROR("UnrealHook: MemoryScanner failed to initialize.");
        return false;
    }

    // Example AoB signature for ULocalPlayer::GetProjectionData in UE 4.27/5.0
    // Note: In reality, we'd fall back to multiple signatures or use a config profile.
    const std::string sig_GetProjectionData = "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 8B F2 48 8B F9 48 8D";
    
    uint8_t* pGetProjectionData = MemoryScanner::Get().ScanSignature(sig_GetProjectionData);
    if (!pGetProjectionData) {
        LOG_WARN("UnrealHook: Failed to find GetProjectionData signature. Falling back to Universal Mode.");
        return false;
    }

    LOG_INFO("UnrealHook: Found GetProjectionData at %p", pGetProjectionData);

    if (MH_CreateHook(pGetProjectionData, reinterpret_cast<LPVOID>(&DetourGetProjectionData), reinterpret_cast<LPVOID*>(&m_originalGetProjectionData)) != MH_OK) {
        LOG_ERROR("UnrealHook: Failed to create hook for GetProjectionData.");
        return false;
    }

    if (MH_EnableHook(pGetProjectionData) != MH_OK) {
        LOG_ERROR("UnrealHook: Failed to enable hook for GetProjectionData.");
        return false;
    }

    LOG_INFO("UnrealHook: Successfully hooked GetProjectionData!");
    return true;
}

void UnrealHook::Shutdown() {
    LOG_INFO("UnrealHook::Shutdown()");
    // MH_DisableHook(MH_ALL_HOOKS) is now handled via MH_Uninitialize in HookManager
}

} // namespace ue
} // namespace vrinject
