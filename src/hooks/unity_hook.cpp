#include "unity_hook.h"
#include "../core/logger.h"
#include "../core/memory_scanner.h"
#include "../core/engine_scanners/unity_scanner.h"
#include "../rendering/openxr_manager.h"
#include "../core/config_manager.h"
#include "MinHook.h"
#include <DirectXMath.h>

using namespace DirectX;

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

    // VR Pose Injection
    auto* openxr = OpenXRManager::GetInstance();
    if (openxr && openxr->IsSessionRunning()) {
        XrPosef eyePose;
        if (openxr->GetEyePose(0, eyePose)) {
            // Convert XrPose to DirectXMath View Matrix
            // OpenXR: +Y up, +X right, -Z forward
            // Unity:  +Y up, +X right, -Z forward (Unity camera space is Right-Handed!)
            // Wait, Unity camera space is RH (-Z forward), but World is LH (+Z forward).
            // WorldToCamera matrix converts from LH World to RH Camera.
            // Our VR headset pose is in LH tracking space if we treat it as world? 
            // Actually, OpenXR is RH. So XrPose is RH (+Y up, +X right, -Z forward).
            XMVECTOR rot = XMVectorSet(-eyePose.orientation.x, -eyePose.orientation.y, eyePose.orientation.z, eyePose.orientation.w);
            float scale = ConfigManager::GetInstance().GetConfig().vrScaleFactor;
            XMVECTOR pos = XMVectorSet(eyePose.position.x * scale, eyePose.position.y * scale, -eyePose.position.z * scale, 0.0f);
            
            XMMATRIX vrView = XMMatrixAffineTransformation(
                XMVectorSet(1,1,1,0),
                XMVectorZero(),
                rot,
                pos
            );
            vrView = XMMatrixInverse(nullptr, vrView);

            // Unity's Matrix4x4 is column-major in memory.
            // DirectXMath's XMLoadFloat4x4 expects row-major, so loading column-major data
            // gives us the transpose. We then transpose to get the correct row-major matrix
            // for DirectXMath operations.
            XMMATRIX gameViewRowMajor = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(matrix));
            // gameViewRowMajor now contains the transpose of Unity's matrix (correct for DirectXMath)
            
            // Multiply Game View (World -> Camera) by VR View (Camera -> Eye)
            // Both are in row-major convention for DirectXMath
            XMMATRIX combinedView = XMMatrixMultiply(gameViewRowMajor, vrView);
            
            // Convert back to column-major for Unity
            XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(matrix), XMMatrixTranspose(combinedView));
        }
    }

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

    // VR Projection Injection
    auto* openxr = OpenXRManager::GetInstance();
    if (openxr && openxr->IsSessionRunning()) {
        // We use left eye (0) for the main camera, and right eye is reprojected later
        XrFovf fov;
        if (openxr->GetEyeFov(0, fov)) {
            // Unity's Matrix4x4 is column-major in memory.
            XMMATRIX gameProjColMajor = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(matrix));
            XMMATRIX gameProj = XMMatrixTranspose(gameProjColMajor);
            XMFLOAT4X4 gProj;
            XMStoreFloat4x4(&gProj, gameProj);

            // Reconstruct VR projection mapping X and Y correctly, but keeping Z mapping
            // Note: X and Y scale are at _11 and _22. Asymmetric shifts are at _13 and _23 (row 0/1, col 2)
            gProj._11 = 2.0f / (tanf(fov.angleRight) - tanf(fov.angleLeft));
            gProj._22 = 2.0f / (tanf(fov.angleUp) - tanf(fov.angleDown));
            gProj._13 = -(tanf(fov.angleRight) + tanf(fov.angleLeft)) / (tanf(fov.angleRight) - tanf(fov.angleLeft));
            gProj._23 = -(tanf(fov.angleUp) + tanf(fov.angleDown)) / (tanf(fov.angleUp) - tanf(fov.angleDown));
            
            // Load back to matrix and transpose for Unity
            XMMATRIX vrProj = XMLoadFloat4x4(&gProj);
            XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(matrix), XMMatrixTranspose(vrProj));
        }
    }

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
