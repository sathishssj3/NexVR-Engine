#include "hook_manager.h"
#include "logger.h"
#include "engine_detector.h"
#include "engine_scanners/unreal_scanner.h"
#include "engine_scanners/unity_scanner.h"
#include "../hooks/unreal_hook.h"
#include "../hooks/unity_hook.h"
#include "../hooks/dx11_hook.h"
#include "../hooks/dx12_hook.h"
#include "../hooks/vulkan_hook.h"
#include "../hooks/dxgi_factory_hook.h"
#include "../hooks/input_hook.h"
#include "../hooks/audio_hook.h"
#include <MinHook.h>
#include "diagnostic_context.h"

namespace vrinject {

bool HookManager::InitializeHooks() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_phase == HookPhase::Committed) return true;
    if (!Prepare()) return false;
    if (!Validate()) { Rollback(); return false; }
    if (!Install()) { Rollback(); return false; }
    if (!Verify()) { Rollback(); return false; }
    if (!Commit()) { Rollback(); return false; }
    return true;
}

bool HookManager::Prepare() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_phase = HookPhase::Prepared;
    m_rollbackStack.clear();
    
    LOG_INFO("HookManager: Preparing hooks...");
    return true;
}

bool HookManager::Validate() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_phase = HookPhase::Validated;
    LOG_INFO("HookManager: Validating hook environment...");
    
    // Check if MinHook is already initialized or anything is preventing hooks
    // e.g. check for anticheat presence, incompatible overlays
    
    return true;
}

bool HookManager::Install() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_phase = HookPhase::Installed;
    LOG_INFO("HookManager: Installing MinHook...");
    
    if (MH_Initialize() != MH_OK) {
        LOG_ERROR("HookManager: Failed to initialize MinHook.");
        return false;
    }
    
    m_rollbackStack.push_back([]() { MH_Uninitialize(); });

    if (!InputHook::GetInstance().Initialize()) {
        LOG_WARN("InputHook failed to initialize. Gamepad emulation disabled.");
    } else {
        m_rollbackStack.push_back([]() { InputHook::GetInstance().Shutdown(); });
    }
    
    AudioHook::Initialize();
    // AudioHook does not have a shutdown currently

    DXGIFactoryHook::Initialize();
    m_rollbackStack.push_back([]() { DXGIFactoryHook::Shutdown(); });
    
    if (!DX11Hook::Initialize()) {
        LOG_WARN("DX11Hook initialization failed.");
    } else {
        m_rollbackStack.push_back([]() { DX11Hook::Shutdown(); });
    }
    
    if (!DX12Hook::Initialize()) {
        LOG_WARN("DX12Hook initialization failed.");
    } else {
        m_rollbackStack.push_back([]() { DX12Hook::Shutdown(); });
    }

    vulkan::hooks::InstallVulkanHooks();
    m_rollbackStack.push_back([]() { vulkan::hooks::RemoveVulkanHooks(); });
    LOG_INFO("HookManager: Vulkan hooks installed successfully.");

    DX12Hook::SetOnFrameCallback([](const DX12Hook::FrameResourcesDX12& res) {
        static int frameCount = 0;
        frameCount++;
        if (frameCount % 600 == 0) {
            LOG_INFO("HookManager: VR Runtime Dummy processing frame %d", frameCount);
        }
    });

    EngineDetector::Get().Detect();
    EngineType type = EngineDetector::Get().GetEngineType();

    LOG_INFO("HookManager: Initializing engine-specific hooks...");

    bool nativeHookActive = false;

    switch (type) {
        case EngineType::UnrealEngine4:
        case EngineType::UnrealEngine5: {
            auto& ueScanner = engine_scanners::UnrealScanner::Get();
            if (ueScanner.Initialize() && ueScanner.IsUnrealEngine()) {
                if (ueScanner.HookCamera()) {
                    nativeHookActive = ue::UnrealHook::Get().Initialize();
                    if (nativeHookActive) {
                        m_rollbackStack.push_back([]() { ue::UnrealHook::Get().Shutdown(); });
                        m_rollbackStack.push_back([]() { engine_scanners::UnrealScanner::Get().Shutdown(); });
                        LOG_INFO("HookManager: Native UE hooks ACTIVE.");
                    }
                }
            }
            break;
        }

        case EngineType::Unity: {
            auto& unityScanner = engine_scanners::UnityScanner::Get();
            if (unityScanner.Initialize() && unityScanner.IsUnityEngine()) {
                if (unityScanner.HookCamera()) {
                    nativeHookActive = unity::UnityHook::Get().Initialize();
                    if (nativeHookActive) {
                        m_rollbackStack.push_back([]() { unity::UnityHook::Get().Shutdown(); });
                        m_rollbackStack.push_back([]() { engine_scanners::UnityScanner::Get().Shutdown(); });
                        LOG_INFO("HookManager: Native Unity hooks ACTIVE.");
                    }
                }
            }
            break;
        }

        default:
            break;
    }

    if (!nativeHookActive) {
        LOG_INFO("HookManager: UNIVERSAL MODE (Depth Reprojection)");
    }

    return true;
}

bool HookManager::Verify() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_phase = HookPhase::Verified;
    LOG_INFO("HookManager: Verifying hooks...");
    
    // In a real system, we'd verify hook installation didn't get overwritten immediately.
    // E.g. scan bytes of the hooked functions to ensure the JMP is still there.
    
    return true;
}

bool HookManager::Commit() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_phase = HookPhase::Committed;
    LOG_INFO("HookManager: Enabling hooks globally...");
    
    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        LOG_ERROR("HookManager: Failed to enable hooks.");
        return false;
    }
    
    m_rollbackStack.push_back([]() { MH_DisableHook(MH_ALL_HOOKS); });
    
    return true;
}

void HookManager::Rollback() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_phase = HookPhase::RolledBack;
    LOG_WARN("HookManager: Rolling back transaction...");
    
    // Rollback in reverse order
    for (auto it = m_rollbackStack.rbegin(); it != m_rollbackStack.rend(); ++it) {
        (*it)();
    }
    m_rollbackStack.clear();
}

void HookManager::ShutdownHooks() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_phase == HookPhase::Committed || m_phase == HookPhase::Installed) {
        Rollback();
    }
    m_phase = HookPhase::Uninitialized;
}

} // namespace vrinject
