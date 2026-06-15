#include "unity_hook.h"
#include "../core/logger.h"
#include "../core/memory_scanner.h"
#include "../core/engine_scanners/unity_scanner.h"
#include "MinHook.h"

namespace vrinject {
namespace unity {

// ============================================================================
// Detour: Camera.get_main (static)
// We intercept this to track the main camera instance
// ============================================================================
void* UnityHook::DetourGetMain(void* methodInfo) {
    void* camera = Get().m_originalGetMain(methodInfo);
    
    // Track the main camera instance for subsequent matrix overrides
    if (camera && camera != Get().m_mainCamera) {
        Get().m_mainCamera = camera;
        LOG_INFO("UnityHook: Captured main camera instance at %p", camera);
    }

    return camera;
}

// ============================================================================
// Detour: Camera.set_worldToCameraMatrix
// This is where we inject our VR view matrix
// ============================================================================
void UnityHook::DetourSetWorldToCameraMatrix(void* camera_this, Matrix4x4* matrix, void* methodInfo) {
    static int frameCounter = 0;
    frameCounter++;
    
    if (frameCounter % 300 == 1) {
        LOG_INFO("UnityHook: set_worldToCameraMatrix detour active! Frame %d | Camera: %p", 
            frameCounter, camera_this);
    }

    // TODO: When OpenXR session is active, modify the view matrix here.
    // The incoming matrix is the game's camera-to-world inverse.
    // We need to compose it with the VR headset pose:
    //
    //   Matrix4x4 vrViewMatrix = OpenXRManager::GetViewMatrix(eyeIndex);
    //   Matrix4x4 combinedView = MultiplyMatrices(vrViewMatrix, *matrix);
    //   *matrix = combinedView;

    Get().m_originalSetWorldToCameraMatrix(camera_this, matrix, methodInfo);
}

// ============================================================================
// Detour: Camera.set_projectionMatrix
// This is where we inject our asymmetric VR projection
// ============================================================================
void UnityHook::DetourSetProjectionMatrix(void* camera_this, Matrix4x4* matrix, void* methodInfo) {
    static int frameCounter = 0;
    frameCounter++;

    if (frameCounter % 300 == 1) {
        LOG_INFO("UnityHook: set_projectionMatrix detour active! Frame %d | Camera: %p",
            frameCounter, camera_this);
    }

    // TODO: When OpenXR session is active, override with VR projection matrix.
    // The VR projection is asymmetric (different for left vs right eye).
    //
    //   Matrix4x4 vrProjection = OpenXRManager::GetProjectionMatrix(eyeIndex, nearPlane, farPlane);
    //   *matrix = vrProjection;

    Get().m_originalSetProjectionMatrix(camera_this, matrix, methodInfo);
}

// ============================================================================
// Initialize: Install detours using the addresses found by UnityScanner
// ============================================================================
bool UnityHook::Initialize() {
    LOG_INFO("UnityHook::Initialize() - Attempting to hook Unity Camera functions...");

    auto& scanner = engine_scanners::UnityScanner::Get();

    // Try to hook Camera.get_main first (always useful for tracking)
    void* getMainAddr = scanner.GetCameraGetMain();
    if (getMainAddr) {
        LOG_INFO("UnityHook: Installing Camera.get_main hook at %p", getMainAddr);
        if (MH_CreateHook(getMainAddr,
                          reinterpret_cast<LPVOID>(&DetourGetMain),
                          reinterpret_cast<LPVOID*>(&m_originalGetMain)) != MH_OK) {
            LOG_WARN("UnityHook: Failed to create hook for Camera.get_main.");
        } else if (MH_EnableHook(getMainAddr) != MH_OK) {
            LOG_WARN("UnityHook: Failed to enable hook for Camera.get_main.");
        } else {
            LOG_INFO("UnityHook: Successfully hooked Camera.get_main!");
        }
    }

    // Hook set_worldToCameraMatrix
    void* setWtcAddr = scanner.GetCameraSetWorldToCameraMatrix();
    if (setWtcAddr) {
        LOG_INFO("UnityHook: Installing set_worldToCameraMatrix hook at %p", setWtcAddr);
        if (MH_CreateHook(setWtcAddr,
                          reinterpret_cast<LPVOID>(&DetourSetWorldToCameraMatrix),
                          reinterpret_cast<LPVOID*>(&m_originalSetWorldToCameraMatrix)) != MH_OK) {
            LOG_WARN("UnityHook: Failed to create hook for set_worldToCameraMatrix.");
        } else if (MH_EnableHook(setWtcAddr) != MH_OK) {
            LOG_WARN("UnityHook: Failed to enable hook for set_worldToCameraMatrix.");
        } else {
            LOG_INFO("UnityHook: Successfully hooked set_worldToCameraMatrix!");
        }
    }

    // Hook set_projectionMatrix
    void* setProjAddr = scanner.GetCameraSetProjectionMatrix();
    if (setProjAddr) {
        LOG_INFO("UnityHook: Installing set_projectionMatrix hook at %p", setProjAddr);
        if (MH_CreateHook(setProjAddr,
                          reinterpret_cast<LPVOID>(&DetourSetProjectionMatrix),
                          reinterpret_cast<LPVOID*>(&m_originalSetProjectionMatrix)) != MH_OK) {
            LOG_WARN("UnityHook: Failed to create hook for set_projectionMatrix.");
        } else if (MH_EnableHook(setProjAddr) != MH_OK) {
            LOG_WARN("UnityHook: Failed to enable hook for set_projectionMatrix.");
        } else {
            LOG_INFO("UnityHook: Successfully hooked set_projectionMatrix!");
        }
    }

    // Check if we hooked at least something useful
    m_isHooked = (m_originalGetMain != nullptr) || 
                 (m_originalSetWorldToCameraMatrix != nullptr) ||
                 (m_originalSetProjectionMatrix != nullptr);

    if (m_isHooked) {
        LOG_INFO("UnityHook: Camera hooks installed successfully!");
    } else {
        LOG_WARN("UnityHook: No camera hooks installed. Falling back to Universal Mode.");
    }

    return m_isHooked;
}

void UnityHook::Shutdown() {
    LOG_INFO("UnityHook::Shutdown()");
    m_isHooked = false;
    m_mainCamera = nullptr;
    // MH_DisableHook(MH_ALL_HOOKS) is handled via MH_Uninitialize in HookManager
}

} // namespace unity
} // namespace vrinject
