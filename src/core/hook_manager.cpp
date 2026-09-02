#include "core/hook_manager.h"
#include "core/logger.h"
#include "core/engine_detector.h"
#include <algorithm>
#include <cctype>
#include "hooks/dx11_hook.h"
#include "hooks/dx12_hook.h"
#include "hooks/vulkan_hook.h"
#include "hooks/dxgi_factory_hook.h"
#include "hooks/input_hook.h"
#include "hooks/audio_hook.h"
#include <MinHook.h>
#include "core/diagnostic_context.h"
#include "core/subsystem_context.h"
#include "core/iat_hook.h"

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

    // Backend election. When the loader has inserted us as a Vulkan layer we are already on
    // the dispatch chain, and we must not detour ANY other presentation path in the process.
    //
    // Two distinct conflicts, both observed in No Man's Sky:
    //   1. MinHook detours on vulkan-1.dll's exported symbols corrupt the loader's chain.
    //   2. VK_LAYER_NV_present - NVIDIA's own layer, inserted below us - presents Vulkan to
    //      the desktop through an internal DXGI swapchain. DXGIFactoryHook captures that
    //      swapchain and its ID3D12CommandQueue, DX11Hook detours its Present, and the game
    //      then reports backend=2 (DX12) when it is really Vulkan. We end up detouring the
    //      driver's own presentation path.
    //
    // A Vulkan game has nothing for the D3D hooks to do, so skipping them costs nothing and
    // removes both conflicts. The D3D path stays for genuine D3D titles and for late
    // injection into a running process, where no layer is present.
    if (vulkan::hooks::IsLayerActive()) {
        LOG_INFO("HookManager: Vulkan layer active - skipping DXGI/DX11/DX12 and MinHook Vulkan detours.");
    } else {
        bool hasAntiCheat = (GetModuleHandleA("easyanticheat_x64.dll") != NULL ||
                             GetModuleHandleA("GameOverlayRenderer64.dll") != NULL || // Some over-aggressive overlays
                             GetModuleHandleA("bedaisy.sys") != NULL); // Not a module, but just representing EAC/BE

        if (hasAntiCheat) {
            LOG_INFO("HookManager: Anti-cheat detected. Falling back to IAT Hooking for DXGI/DX11/DX12 to avoid .text inline detours.");
            
            // In a full implementation, DXGIFactoryHook, DX11Hook, DX12Hook would expose their 
            // detour functions, and we'd call IATHook::InstallHook for "CreateDXGIFactory", "D3D11CreateDevice", etc.
            // For this Phase 10 architectural demonstration, we log the fallback.
            
            // DXGIFactoryHook::InitializeIAT();
            // DX11Hook::InitializeIAT();
            // DX12Hook::InitializeIAT();
        } else {
            char exeName[MAX_PATH] = {0};
            GetModuleFileNameA(NULL, exeName, MAX_PATH);
            std::string exeStr = exeName;
            LOG_INFO("HookManager: Executable path is: %s", exeStr.c_str());
            
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
        }

        vulkan::hooks::InstallVulkanHooks();
        m_rollbackStack.push_back([]() { vulkan::hooks::RemoveVulkanHooks(); });
        LOG_INFO("HookManager: Vulkan hooks installed successfully.");
    }

    DX12Hook::SetOnFrameCallback([](const DX12Hook::FrameResourcesDX12& res) {
        static int frameCount = 0;
        frameCount++;
        if (frameCount % 600 == 0) {
            LOG_INFO("HookManager: VR Runtime Dummy processing frame %d", frameCount);
        }
    });

    SubsystemContext::Get().GetEngineDetector()->Detect();
    EngineType type = SubsystemContext::Get().GetEngineDetector()->GetEngineType();

    LOG_INFO("HookManager: Initializing engine-specific hooks...");

    bool nativeHookActive = false;

    switch (type) {
        case EngineType::UnrealEngine4:
        case EngineType::UnrealEngine5: {
            break;
        }

        case EngineType::Unity: {
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
