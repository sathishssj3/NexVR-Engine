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
#include <shlobj.h>    // SHGetFolderPathA

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <atomic>

#pragma comment(lib, "shell32.lib")

// Global module handle – needed to resolve our own DLL path at runtime.
HMODULE g_hModule = nullptr;

// Thread handle for synchronization during shutdown
HANDLE g_hInitThread = nullptr;
std::atomic<bool> g_bIsShuttingDown{false};

namespace {

/// Worker thread that performs all heavy initialization.
/// Any exceptions are caught and logged to avoid process termination.
unsigned __stdcall InitThread(void* /*param*/) {
    try {
        // This DLL installs process-wide hooks. Pin it so an external FreeLibrary
        // cannot unload code while hook trampolines or worker callbacks still point
        // into the module.
        HMODULE pinnedModule = nullptr;
        ::GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCSTR>(&InitThread),
            &pinnedModule);

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

        // Use local app data for log file (works on all machines)
        char logPath[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, logPath))) {
            strcat_s(logPath, "\\VRInject");
            ::CreateDirectoryA(logPath, nullptr);
            strcat_s(logPath, "\\vrinject.log");
        } else {
            // Fallback: write the log next to the DLL itself (works on any drive/path).
            strcpy_s(logPath, dllDir);
            strcat_s(logPath, "vrinject.log");
        }
        vrinject::Logger::Init(logPath);

        LOG_INFO("========================================");
        LOG_INFO("VRInject Framework v0.1 - Initializing");
        LOG_INFO("========================================");
        LOG_INFO("Module base: 0x%p", static_cast<void*>(g_hModule));

// Initialize trial system (must be done after loader lock released)
        if (!vrinject::TrialManager::GetInstance().InitializeTrial()) {
            LOG_ERROR("Trial validation failed - shutting down");
            return 0;
        }

        if (vrinject::HookManager::Get().InitializeHooks()) {
            LOG_INFO("Engine-specific hook initialization complete.");
        } else {
            LOG_ERROR("Engine-specific hook initialization FAILED.");
        }

        return 0;
    } catch (const std::exception& e) {
        // Catch any STL exceptions to prevent process crash
        vrinject::Logger::Init("vrinject_error.log");
        LOG_ERROR("InitThread exception: %s", e.what());
        return 1;
    } catch (...) {
        // Catch any other exceptions
        vrinject::Logger::Init("vrinject_error.log");
        LOG_ERROR("InitThread unknown exception");
        return 1;
    }
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
                // Wait up to 2 seconds for init thread to safely exit, preventing half-installed hooks.
                // We use a timeout to avoid a hard deadlock on the loader lock if the thread is stuck.
                WaitForSingleObject(g_hInitThread, 2000);
                CloseHandle(g_hInitThread);
                g_hInitThread = nullptr;
            }
            // DllMain runs under the loader lock. Waiting for threads, removing
            // hooks, destroying COM/graphics objects, or logging here can
            // deadlock against code that needs the same lock. The module is
            // pinned above, so normal FreeLibrary unload is intentionally
            // disabled; process termination lets Windows reclaim resources.
            break;
        }

        default:
            break;
    }

    return TRUE;
}
