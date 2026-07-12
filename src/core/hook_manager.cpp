#include "hook_manager.h"
#include "logger.h"
#include "engine_detector.h"
#include "engine_scanners/unreal_scanner.h"
#include "engine_scanners/unity_scanner.h"
#include "../hooks/unreal_hook.h"
#include "../hooks/unity_hook.h"
#include "../hooks/dx11_hook.h"
#include "../hooks/dx12_hook.h"
#include "../hooks/dxgi_factory_hook.h"
#include "../hooks/input_hook.h"
#include "../hooks/audio_hook.h"
#include "../hooks/MinHook.h"

namespace vrinject {

bool HookManager::InitializeHooks() {
    LOG_INFO("HookManager: Initializing MinHook...");
    if (MH_Initialize() != MH_OK) {
        LOG_ERROR("HookManager: Failed to initialize MinHook.");
        return false;
    }

    // Setup Core Windows & Input Hooks first
    if (!InputHook::GetInstance().Initialize()) {
        LOG_WARN("InputHook failed to initialize. Gamepad emulation disabled.");
    }
    
    AudioHook::Initialize();

    DXGIFactoryHook::Initialize();
    // Setup Graphics Pipeline Hooks
    // DX11 must hook Present FIRST because its ProcessPresent has routing logic
    // to detect DX12 swapchains and forward them to DX12Hook::OnPresent.
    // Both DX11 and DX12 share the same IDXGISwapChain::Present vtable entry,
    // so only one can own the hook — DX11's handler correctly dispatches both.
    if (!DX11Hook::Initialize()) {
        LOG_WARN("DX11Hook initialization failed.");
    }
    DX12Hook::Initialize();

    DX12Hook::SetOnFrameCallback([](const DX12Hook::FrameResourcesDX12& res) {
        // Dummy implementation to verify callback is firing
        static int frameCount = 0;
        frameCount++;
        if (frameCount % 600 == 0) {
            LOG_INFO("HookManager: VR Runtime Dummy processing frame %d", frameCount);
        }
    });

    // ========================================================================
    // Engine Detection & Native Camera Hooking
    // ========================================================================
    EngineDetector::Get().Detect();
    EngineType type = EngineDetector::Get().GetEngineType();

    LOG_INFO("HookManager: Initializing engine-specific hooks...");

    bool nativeHookActive = false;

    switch (type) {
        case EngineType::UnrealEngine4:
        case EngineType::UnrealEngine5: {
            LOG_INFO("HookManager: Engine detected as Unreal Engine. Applying UE hooks.");
            
            // Phase 1: Run the memory scanner to find GWorld, GEngine, and camera functions
            auto& ueScanner = engine_scanners::UnrealScanner::Get();
            if (ueScanner.Initialize() && ueScanner.IsUnrealEngine()) {
                // Phase 2: Attempt to hook the camera function
                if (ueScanner.HookCamera()) {
                    // Phase 3: Install the MinHook detour
                    nativeHookActive = ue::UnrealHook::Get().Initialize();
                    if (nativeHookActive) {
                        LOG_INFO("HookManager: Native Unreal Engine camera hook is ACTIVE! "
                                 "Head tracking will use engine-native projection.");
                    }
                }
            }
            
            if (!nativeHookActive) {
                LOG_WARN("HookManager: Unreal Engine native hooks failed. "
                         "Falling back to Universal Mode (depth reprojection). "
                         "This may result in less accurate 3D, but VR will still work.");
            }
            break;
        }

        case EngineType::Unity: {
            LOG_INFO("HookManager: Engine detected as Unity. Applying Unity hooks.");
            
            // Phase 1: Detect backend and resolve scripting APIs
            auto& unityScanner = engine_scanners::UnityScanner::Get();
            if (unityScanner.Initialize() && unityScanner.IsUnityEngine()) {
                // Phase 2: Find Camera class methods
                if (unityScanner.HookCamera()) {
                    // Phase 3: Install MinHook detours on the camera methods
                    nativeHookActive = unity::UnityHook::Get().Initialize();
                    if (nativeHookActive) {
                        LOG_INFO("HookManager: Native Unity camera hook is ACTIVE! "
                                 "Head tracking will use engine-native projection.");
                    }
                }
            }

            if (!nativeHookActive) {
                LOG_WARN("HookManager: Unity native hooks failed. "
                         "Falling back to Universal Mode (depth reprojection). "
                         "This may result in less accurate 3D, but VR will still work.");
            }
            break;
        }

        case EngineType::Unknown:
        default:
            LOG_INFO("HookManager: Engine type unknown. Relying solely on Universal Injection "
                     "(Matrix Classification & Depth Reprojection).");
            break;
    }

    // Log the final injection mode
    if (nativeHookActive) {
        LOG_INFO("===================================================");
        LOG_INFO("  NexVR Engine: NATIVE ENGINE HOOK MODE ACTIVE");
        LOG_INFO("  Engine: %s", EngineDetector::Get().GetEngineVersionString().c_str());
        LOG_INFO("===================================================");
    } else {
        LOG_INFO("===================================================");
        LOG_INFO("  NexVR Engine: UNIVERSAL MODE (Depth Reprojection)");
        LOG_INFO("===================================================");
    }

    return true;
}

void HookManager::ShutdownHooks() {
    ue::UnrealHook::Get().Shutdown();
    unity::UnityHook::Get().Shutdown();
    engine_scanners::UnrealScanner::Get().Shutdown();
    engine_scanners::UnityScanner::Get().Shutdown();
    
    LOG_INFO("HookManager: Uninitializing MinHook...");
    MH_Uninitialize();
}

} // namespace vrinject
