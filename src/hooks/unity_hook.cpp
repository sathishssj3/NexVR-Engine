#include "unity_hook.h"
#include "../core/logger.h"

#include "../core/memory_scanner.h"
#include "MinHook.h"

namespace vrinject {
namespace unity {

void UnityHook::DetourSetWorldToCameraMatrix(void* camera_this, void* matrix, void* method_info) {
    // 1. We could modify the incoming matrix with our OpenXR head pose
    // Or ignore the incoming matrix completely and supply our own!
    
    // Example: matrix = OpenXRManager::Get().GetViewMatrix();

    Get().m_originalSetWorldToCameraMatrix(camera_this, matrix, method_info);
}

void UnityHook::DetourSetProjectionMatrix(void* camera_this, void* matrix, void* method_info) {
    // Modify projection matrix for VR
    Get().m_originalSetProjectionMatrix(camera_this, matrix, method_info);
}

bool UnityHook::Initialize() {
    LOG_INFO("UnityHook::Initialize() - Attempting to hook Unity Camera functions...");
    
    if (!MemoryScanner::Get().Initialize()) {
        LOG_ERROR("UnityHook: MemoryScanner failed to initialize.");
        return false;
    }

    // Typical signature for UnityEngine.Camera::set_worldToCameraMatrix in Unity 2021+ IL2CPP
    const std::string sig_SetWorldToCameraMatrix = "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 8B F2 48 8B F9 48 8B 0D";
    const std::string sig_SetProjectionMatrix = "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 41 56 41 57 48 83 EC ? 48 8B F2";

    uint8_t* pSetWorldToCameraMatrix = MemoryScanner::Get().ScanSignature(sig_SetWorldToCameraMatrix, "GameAssembly.dll");
    uint8_t* pSetProjectionMatrix = MemoryScanner::Get().ScanSignature(sig_SetProjectionMatrix, "GameAssembly.dll");

    if (!pSetWorldToCameraMatrix || !pSetProjectionMatrix) {
        LOG_WARN("UnityHook: Failed to find Camera matrix setters in GameAssembly.dll. Falling back to Universal Mode.");
        return false;
    }

    LOG_INFO("UnityHook: Found set_worldToCameraMatrix at %p", pSetWorldToCameraMatrix);
    LOG_INFO("UnityHook: Found set_projectionMatrix at %p", pSetProjectionMatrix);

    if (MH_CreateHook(pSetWorldToCameraMatrix, reinterpret_cast<LPVOID>(&DetourSetWorldToCameraMatrix), reinterpret_cast<LPVOID*>(&m_originalSetWorldToCameraMatrix)) != MH_OK) {
        LOG_ERROR("UnityHook: Failed to create hook for set_worldToCameraMatrix.");
        return false;
    }

    if (MH_CreateHook(pSetProjectionMatrix, reinterpret_cast<LPVOID>(&DetourSetProjectionMatrix), reinterpret_cast<LPVOID*>(&m_originalSetProjectionMatrix)) != MH_OK) {
        LOG_ERROR("UnityHook: Failed to create hook for set_projectionMatrix.");
        return false;
    }

    if (MH_EnableHook(pSetWorldToCameraMatrix) != MH_OK || MH_EnableHook(pSetProjectionMatrix) != MH_OK) {
        LOG_ERROR("UnityHook: Failed to enable hooks.");
        return false;
    }

    LOG_INFO("UnityHook: Successfully hooked IL2CPP Camera setters!");
    return true;
}

void UnityHook::Shutdown() {
    LOG_INFO("UnityHook::Shutdown()");
    // MH_DisableHook(MH_ALL_HOOKS) is now handled via MH_Uninitialize in HookManager
}

} // namespace unity
} // namespace vrinject
