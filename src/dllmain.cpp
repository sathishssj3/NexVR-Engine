// ============================================================================
// dllmain.cpp – VRInject DLL entry point
//
// Initialization is done on a dedicated thread because:
//   1. The DllMain callback runs under the Windows loader lock.
//   2. Hook installation (MinHook + D3D device creation) may call LoadLibrary
//      internally, which would deadlock if we held the loader lock.
//   3. Some target applications create their D3D device lazily; we need to
//      wait/poll, which must not block DllMain.
// ============================================================================

#include "core/logger.h"
#include "hooks/dx11_hook.h"
#include "hooks/dx12_hook.h"
#include "hooks/input_hook.h"
#include "core/hook_manager.h"
#include "core/config_manager.h"
#include "core/drm_manager.h"

#include <process.h>   // _beginthreadex

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <atomic>

// Global module handle – needed to resolve our own DLL path at runtime.
HMODULE g_hModule = nullptr;

// Thread handle for synchronization during shutdown
HANDLE g_hInitThread = nullptr;
std::atomic<bool> g_bIsShuttingDown{false};

namespace {

/// Worker thread that performs all heavy initialization.
unsigned __stdcall InitThread(void* /*param*/) {
    // Give the host process a moment to finish its own init. Some games
    // crash if we create a D3D dummy device too early.
    for (int i = 0; i < 50; ++i) {
        if (g_bIsShuttingDown) return 0;
        ::Sleep(10);
    }

    // Resolve the directory our DLL lives in so the log file lands next to it.
    char dllDir[MAX_PATH]{};
    if (::GetModuleFileNameA(g_hModule, dllDir, MAX_PATH)) {
        // Strip the filename, keep the directory.
        char* lastSlash = std::strrchr(dllDir, '\\');
        if (lastSlash) *(lastSlash + 1) = '\0';
    }

    std::string logPath = "C:\\Users\\sathi\\vrinject.log";
    vrinject::Logger::Init(logPath);

    LOG_INFO("========================================");
    LOG_INFO("VRInject Framework v0.1 - Initializing");
    LOG_INFO("========================================");
    LOG_INFO("Module base: 0x%p", static_cast<void*>(g_hModule));

    if (vrinject::HookManager::Get().InitializeHooks()) {
        LOG_INFO("Engine-specific hook initialization complete.");
    } else {
        LOG_ERROR("Engine-specific hook initialization FAILED.");
    }

    return 0;
}

} // anonymous namespace

// ============================================================================
// DllMain
// ============================================================================
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH: {
            // We never need per-thread attach/detach notifications.
            ::DisableThreadLibraryCalls(hModule);

            g_hModule = hModule;

            if (!vrinject::TrialManager::GetInstance().CheckAndEnforceTrial()) {
                return FALSE;
            }

            // Kick off initialization on a separate thread.
            // _beginthreadex is safer than CreateThread when the CRT is in use.
            uintptr_t hThread = _beginthreadex(
                nullptr,        // security
                0,              // stack size (default)
                InitThread,
                nullptr,        // param
                0,              // flags – start immediately
                nullptr         // thread id (don't care)
            );

            if (hThread) {
                // Keep the handle for synchronization on unload
                g_hInitThread = reinterpret_cast<HANDLE>(hThread);
            }
            break;
        }

        case DLL_PROCESS_DETACH: {
            g_bIsShuttingDown = true;
            if (g_hInitThread) {
                ::WaitForSingleObject(g_hInitThread, 1000);
                ::CloseHandle(g_hInitThread);
                g_hInitThread = nullptr;
            }

            if (reserved != nullptr) {
                // Process is terminating. Do not clean up COM/DirectX/User32 resources
                // to avoid deadlocking the Windows Loader lock.
                break;
            }
            vrinject::TrialManager::GetInstance().Shutdown();
            vrinject::InputHook::GetInstance().Shutdown();
            LOG_INFO("VRInject Framework - Shutting down");
            vrinject::DX11Hook::Shutdown();
            vrinject::DX12Hook::Shutdown();
            LOG_INFO("Shutdown complete - goodbye.");
            vrinject::HookManager::Get().ShutdownHooks();
            vrinject::Logger::Shutdown();
            break;
        }

        default:
            break;
    }

    return TRUE;
}
