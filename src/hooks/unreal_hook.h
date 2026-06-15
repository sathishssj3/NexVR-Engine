#pragma once

#include "../core/ue_sdk.h"
#include <cstdint>

namespace vrinject {
namespace ue {

class UnrealHook {
public:
    static UnrealHook& Get() {
        static UnrealHook instance;
        return instance;
    }

    bool Initialize();
    void Shutdown();

    // Whether the native UE camera hook is active
    bool IsHooked() const { return m_isHooked; }

private:
    UnrealHook() = default;
    
    // --- GetProjectionData detour (UE4) ---
    // Signature: bool ULocalPlayer::GetProjectionData(FViewport*, EStereoscopicPass, FSceneViewProjectionData&)
    typedef bool (*GetProjectionData_t)(void* pLocalPlayer, void* pViewport, int stereoPass, void* pProjectionData);
    GetProjectionData_t m_originalGetProjectionData = nullptr;
    static bool DetourGetProjectionData(void* pLocalPlayer, void* pViewport, int stereoPass, void* pProjectionData);

    // --- CalcSceneView detour (UE5) ---
    // In UE5, CalcSceneView replaces GetProjectionData with a different signature.
    // We use the same typedef for simplicity since we only need to modify the output.
    typedef void* (*CalcSceneView_t)(void* pLocalPlayer, void* pOutViewInfo, void* pOutProjectionData, 
                                      void* pViewport, void* pViewDrawer, int stereoViewIndex);
    CalcSceneView_t m_originalCalcSceneView = nullptr;
    static void* DetourCalcSceneView(void* pLocalPlayer, void* pOutViewInfo, void* pOutProjectionData,
                                      void* pViewport, void* pViewDrawer, int stereoViewIndex);

    // --- UpdateCamera detour (fallback for both UE4/5) ---
    typedef void (*UpdateCamera_t)(void* pCameraManager, float deltaTime);
    UpdateCamera_t m_originalUpdateCamera = nullptr;
    static void DetourUpdateCamera(void* pCameraManager, float deltaTime);

    bool m_isHooked = false;

    // Track which hook type is active
    enum class HookType { None, GetProjectionData, CalcSceneView, UpdateCamera };
    HookType m_activeHookType = HookType::None;
};

} // namespace ue
} // namespace vrinject
