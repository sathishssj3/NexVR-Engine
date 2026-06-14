#include "hook_manager.h"
#include "logger.h"
#include "engine_detector.h"
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
    DX12Hook::Initialize();

    DX12Hook::SetOnFrameCallback([](const DX12Hook::FrameResourcesDX12& res) {
        // Dummy implementation to verify callback is firing
        static int frameCount = 0;
        frameCount++;
        // We only log once in OnPresent to avoid spam, but we can log every 100 frames here
        if (frameCount % 600 == 0) {
            LOG_INFO("HookManager: VR Runtime Dummy processing frame %d", frameCount);
        }
    });

    // Setup Graphics Pipeline Hook (DXGI + DX11)
    if (!DX11Hook::Initialize()) {
        LOG_WARN("DX11Hook initialization failed. This is expected for DX12-only games.");
    }

    EngineDetector::Get().Detect();
    EngineType type = EngineDetector::Get().GetEngineType();

    LOG_INFO("HookManager: Initializing engine-specific hooks...");

    switch (type) {
        case EngineType::UnrealEngine4:
        case EngineType::UnrealEngine5:
            LOG_INFO("HookManager: Engine detected as Unreal Engine. Applying UE hooks.");
            // ue::UnrealHook::Get().Initialize(); // DISABLED TO ISOLATE CRASH
            break;

        case EngineType::Unity:
            LOG_INFO("HookManager: Engine detected as Unity. Applying Unity hooks.");
            unity::UnityHook::Get().Initialize();
            break;

        case EngineType::Unknown:
        default:
            LOG_INFO("HookManager: Engine type unknown. Relying solely on Universal Injection (Matrix Classification & Reprojection).");
            break;
    }

    return true;
}

void HookManager::ShutdownHooks() {
    ue::UnrealHook::Get().Shutdown();
    unity::UnityHook::Get().Shutdown();
    
    LOG_INFO("HookManager: Uninitializing MinHook...");
    MH_Uninitialize();
}

} // namespace vrinject
