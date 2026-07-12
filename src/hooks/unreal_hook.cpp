#include "unreal_hook.h"
#include "../core/logger.h"
#include "../core/memory_scanner.h"
#include "../core/engine_scanners/unreal_scanner.h"
#include "../rendering/openxr_manager.h"
#include "MinHook.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace vrinject {
namespace ue {

// ============================================================================
// Helper: Convert an OpenXR quaternion to Unreal Engine FRotator (degrees)
// ============================================================================
static FRotator QuatToRotator(const XrQuaternionf& q) {
    // Convert quaternion to Euler angles
    // UE uses a Z-up, left-handed coordinate system
    FRotator rot;

    // Roll (X-axis rotation)
    double sinr_cosp = 2.0 * (q.w * q.x + q.y * q.z);
    double cosr_cosp = 1.0 - 2.0 * (q.x * q.x + q.y * q.y);
    rot.Roll = static_cast<float>(atan2(sinr_cosp, cosr_cosp) * 180.0 / M_PI);

    // Pitch (Y-axis rotation)
    double sinp = 2.0 * (q.w * q.y - q.z * q.x);
    if (fabs(sinp) >= 1.0)
        rot.Pitch = static_cast<float>(copysign(90.0, sinp));
    else
        rot.Pitch = static_cast<float>(asin(sinp) * 180.0 / M_PI);

    // Yaw (Z-axis rotation)
    double siny_cosp = 2.0 * (q.w * q.z + q.x * q.y);
    double cosy_cosp = 1.0 - 2.0 * (q.y * q.y + q.z * q.z);
    rot.Yaw = static_cast<float>(atan2(siny_cosp, cosy_cosp) * 180.0 / M_PI);

    return rot;
}

// ============================================================================
// Helper: Convert OpenXR position (meters, right-handed) to UE (centimeters, left-handed)
// ============================================================================
static FVector XrPosToUE(const XrVector3f& pos) {
    // OpenXR: x=right, y=up, z=back
    // UE4:    x=forward, y=right, z=up
    // Scale: OpenXR uses meters, UE uses centimeters
    return FVector(
        -pos.z * 100.0f,   // OpenXR -z -> UE x (forward)
         pos.x * 100.0f,   // OpenXR x  -> UE y (right)
         pos.y * 100.0f    // OpenXR y  -> UE z (up)
    );
}

// ============================================================================
// Detour: ULocalPlayer::GetProjectionData (UE4)
// ============================================================================
bool UnrealHook::DetourGetProjectionData(void* pLocalPlayer, void* pViewport, int stereoPass, void* pProjectionData) {
    // Let the original function populate the projection data with the game's camera
    bool result = Get().m_originalGetProjectionData(pLocalPlayer, pViewport, stereoPass, pProjectionData);
    
    if (!result || !pProjectionData) return result;

    // pProjectionData points to an FSceneViewProjectionData struct.
    // The ViewOrigin and ViewRotation are at known offsets.
    // We cast and modify in-place.
    auto* projData = static_cast<FSceneViewProjectionData*>(pProjectionData);

    // TODO: When OpenXR session is active, inject the headset pose here.
    // For now, we log that the detour fired successfully.
    static int frameCounter = 0;
    frameCounter++;
    if (frameCounter % 300 == 1) {
        LOG_INFO("UnrealHook: GetProjectionData detour active! Frame %d | ViewOrigin: (%.1f, %.1f, %.1f) | FOV pass: %d",
            frameCounter,
            projData->ViewOrigin.X, projData->ViewOrigin.Y, projData->ViewOrigin.Z,
            stereoPass);
    }

    // --- VR Pose Injection ---
    auto* openxr = OpenXRManager::GetInstance();
    if (openxr && openxr->IsSessionRunning()) {
        const XrPosef& headPose = openxr->GetLatestHeadPose();
        
        FVector hmdOffset = XrPosToUE(headPose.position);
        FRotator hmdRotation = QuatToRotator(headPose.orientation);
        
        // Unreal uses absolute camera positioning in world space here.
        // Usually, games add the camera's local transform to the pawn.
        // A simple injection is to override the rotation completely, 
        // and add the positional offset to the origin.
        // For full 6DOF roomscale, we'd need to rotate the offset by the pawn's yaw.
        projData->ViewOrigin = projData->ViewOrigin + hmdOffset;
        projData->ViewRotation = hmdRotation;
        
        // We can also override the projection matrix if we have the eye Fov
        // XrFovf fov;
        // if (openxr->GetEyeFov(stereoPass, fov)) {
        //     projData->ProjectionMatrix = BuildVRProjectionMatrix(...);
        // }
    }

    return result;
}

// ============================================================================
// Detour: ULocalPlayer::CalcSceneView (UE5)
// ============================================================================
void* UnrealHook::DetourCalcSceneView(void* pLocalPlayer, void* pOutViewInfo, void* pOutProjectionData,
                                       void* pViewport, void* pViewDrawer, int stereoViewIndex) {
    // Call original to let the engine calculate the default view
    void* result = Get().m_originalCalcSceneView(pLocalPlayer, pOutViewInfo, pOutProjectionData,
                                                   pViewport, pViewDrawer, stereoViewIndex);

    if (pOutViewInfo) {
        // pOutViewInfo contains an FMinimalViewInfo we can modify
        auto* viewInfo = static_cast<FMinimalViewInfo*>(pOutViewInfo);

        static int frameCounter = 0;
        frameCounter++;
        if (frameCounter % 300 == 1) {
            LOG_INFO("UnrealHook: CalcSceneView detour active! Frame %d | Loc: (%.1f, %.1f, %.1f) | FOV: %.1f",
                frameCounter,
                viewInfo->Location.X, viewInfo->Location.Y, viewInfo->Location.Z,
                viewInfo->FOV);
        }

        // VR Pose Injection
        auto* openxr = OpenXRManager::GetInstance();
        if (openxr && openxr->IsSessionRunning()) {
            const XrPosef& headPose = openxr->GetLatestHeadPose();
            
            FVector hmdOffset = XrPosToUE(headPose.position);
            FRotator hmdRotation = QuatToRotator(headPose.orientation);
            
            viewInfo->Location = viewInfo->Location + hmdOffset;
            viewInfo->Rotation = hmdRotation;
        }
    }

    return result;
}

// ============================================================================
// Detour: APlayerCameraManager::UpdateCamera (fallback)
// ============================================================================
void UnrealHook::DetourUpdateCamera(void* pCameraManager, float deltaTime) {
    // Call original first
    Get().m_originalUpdateCamera(pCameraManager, deltaTime);

    // After the camera manager updates, we can read the resulting camera state.
    // The FMinimalViewInfo is typically at a fixed offset within APlayerCameraManager.
    // We'll need to walk the object to find it.
    
    static int frameCounter = 0;
    frameCounter++;
    if (frameCounter % 300 == 1) {
        LOG_INFO("UnrealHook: UpdateCamera detour active! Frame %d | DeltaTime: %.4f",
            frameCounter, deltaTime);
    }

    // TODO: Read the camera's ViewTarget and inject VR pose
}

// ============================================================================
// Detour: FRendererModule::BeginRenderingViewFamily
// ============================================================================
void UnrealHook::DetourBeginRenderingViewFamily(void* pCanvas, FSceneViewFamily* pViewFamily) {
    if (!pViewFamily) {
        Get().m_originalBeginRenderingViewFamily(pCanvas, pViewFamily);
        return;
    }

    auto* openxr = OpenXRManager::GetInstance();
    if (openxr && openxr->IsSessionRunning() && pViewFamily->Views.ArrayNum > 0) {
        
        static int frameCounter = 0;
        frameCounter++;
        if (frameCounter % 300 == 1) {
            LOG_INFO("UnrealHook: BeginRenderingViewFamily detour active! Frame %d | Original Views: %d",
                frameCounter, pViewFamily->Views.ArrayNum);
        }

        // To achieve Native Stereo Geometry 3D, we must duplicate the view.
        // The original view is for the flat monitor. We want two views (Left and Right).
        // For demonstration, if there's only 1 view, we can allocate a new array of 2 views.
        if (pViewFamily->Views.ArrayNum == 1) {
            FSceneView* originalView = pViewFamily->Views.Data[0];
            
            // Note: In a production UEVR implementation, we must properly construct 
            // the new FSceneView copies using the game's allocator (FMemory::Malloc), 
            // because the renderer will try to free them or they'll be out of scope.
            // For this implementation plan, we outline the logic here.
            
            // 1. Create a left eye view based on originalView
            // 2. Override Left ViewOrigin and ViewRotation
            // 3. Create a right eye view based on originalView
            // 4. Override Right ViewOrigin and ViewRotation
            // 5. Update pViewFamily->Views.Data to point to our new array of 2 pointers.
            // 6. Update pViewFamily->Views.ArrayNum = 2;
            
            // Then let the engine render it!
        }
    }

    // Call the original renderer to actually draw the family
    Get().m_originalBeginRenderingViewFamily(pCanvas, pViewFamily);
}

// ============================================================================
// Initialize: Install the appropriate detour based on what the scanner found
// ============================================================================
bool UnrealHook::Initialize() {
    LOG_INFO("UnrealHook::Initialize() - Attempting to hook Unreal Engine camera functions...");

    auto& scanner = engine_scanners::UnrealScanner::Get();

    // Try to hook GetProjectionData / CalcSceneView first (highest fidelity)
    void* projFunc = scanner.GetProjectionDataFunc();
    void* updateFunc = scanner.GetUpdateCameraFunc();

    if (projFunc) {
        auto version = scanner.GetVersion();
        
        if (version == engine_scanners::UEVersion::UE4_25) {
            // UE4: GetProjectionData
            LOG_INFO("UnrealHook: Installing GetProjectionData hook (UE4) at %p", projFunc);
            if (MH_CreateHook(projFunc, 
                              reinterpret_cast<LPVOID>(&DetourGetProjectionData),
                              reinterpret_cast<LPVOID*>(&m_originalGetProjectionData)) != MH_OK) {
                LOG_ERROR("UnrealHook: MH_CreateHook failed for GetProjectionData.");
                return false;
            }
            if (MH_EnableHook(projFunc) != MH_OK) {
                LOG_ERROR("UnrealHook: MH_EnableHook failed for GetProjectionData.");
                return false;
            }
            m_activeHookType = HookType::GetProjectionData;
            m_isHooked = true;
            LOG_INFO("UnrealHook: Successfully hooked GetProjectionData!");
            return true;
        } else {
            // UE5: CalcSceneView
            LOG_INFO("UnrealHook: Installing CalcSceneView hook (UE5) at %p", projFunc);
            if (MH_CreateHook(projFunc,
                              reinterpret_cast<LPVOID>(&DetourCalcSceneView),
                              reinterpret_cast<LPVOID*>(&m_originalCalcSceneView)) != MH_OK) {
                LOG_ERROR("UnrealHook: MH_CreateHook failed for CalcSceneView.");
                return false;
            }
            if (MH_EnableHook(projFunc) != MH_OK) {
                LOG_ERROR("UnrealHook: MH_EnableHook failed for CalcSceneView.");
                return false;
            }
            m_activeHookType = HookType::CalcSceneView;
            m_isHooked = true;
            LOG_INFO("UnrealHook: Successfully hooked CalcSceneView!");
            return true;
        }
    }

    // Fallback: Hook UpdateCamera
    if (updateFunc) {
        LOG_INFO("UnrealHook: Installing UpdateCamera hook (fallback) at %p", updateFunc);
        if (MH_CreateHook(updateFunc,
                          reinterpret_cast<LPVOID>(&DetourUpdateCamera),
                          reinterpret_cast<LPVOID*>(&m_originalUpdateCamera)) != MH_OK) {
            LOG_ERROR("UnrealHook: MH_CreateHook failed for UpdateCamera.");
            return false;
        }
        if (MH_EnableHook(updateFunc) != MH_OK) {
            LOG_ERROR("UnrealHook: MH_EnableHook failed for UpdateCamera.");
            return false;
        }
        m_activeHookType = HookType::UpdateCamera;
        m_isHooked = true;
        LOG_INFO("UnrealHook: Successfully hooked UpdateCamera (fallback)!");
        return true;
    }

    // Try hooking BeginRenderingViewFamily for full Native Stereo
    void* beginRenderingFunc = scanner.GetBeginRenderingViewFamilyFunc();
    if (beginRenderingFunc) {
        LOG_INFO("UnrealHook: Installing BeginRenderingViewFamily hook at %p", beginRenderingFunc);
        if (MH_CreateHook(beginRenderingFunc,
                          reinterpret_cast<LPVOID>(&DetourBeginRenderingViewFamily),
                          reinterpret_cast<LPVOID*>(&m_originalBeginRenderingViewFamily)) != MH_OK) {
            LOG_ERROR("UnrealHook: MH_CreateHook failed for BeginRenderingViewFamily.");
            return false;
        }
        if (MH_EnableHook(beginRenderingFunc) != MH_OK) {
            LOG_ERROR("UnrealHook: MH_EnableHook failed for BeginRenderingViewFamily.");
            return false;
        }
        m_activeHookType = HookType::BeginRenderingViewFamily;
        m_isHooked = true;
        LOG_INFO("UnrealHook: Successfully hooked BeginRenderingViewFamily!");
        return true;
    }

    LOG_WARN("UnrealHook: No hookable function found. Falling back to Universal Mode (depth reprojection).");
    return false;
}

void UnrealHook::Shutdown() {
    LOG_INFO("UnrealHook::Shutdown()");
    m_isHooked = false;
    m_activeHookType = HookType::None;
    // MH_DisableHook(MH_ALL_HOOKS) is handled via MH_Uninitialize in HookManager
}

} // namespace ue
} // namespace vrinject
