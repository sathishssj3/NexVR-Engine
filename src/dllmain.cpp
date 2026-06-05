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

#include <process.h>   // _beginthreadex

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

// Global module handle – needed to resolve our own DLL path at runtime.
HMODULE g_hModule = nullptr;

namespace {

/// Worker thread that performs all heavy initialization.
unsigned __stdcall InitThread(void* /*param*/) {
    // Give the host process a moment to finish its own init. Some games
    // crash if we create a D3D dummy device too early.
    ::Sleep(500);

    // Resolve the directory our DLL lives in so the log file lands next to it.
    char dllDir[MAX_PATH]{};
    if (::GetModuleFileNameA(g_hModule, dllDir, MAX_PATH)) {
        // Strip the filename, keep the directory.
        char* lastSlash = std::strrchr(dllDir, '\\');
        if (lastSlash) *(lastSlash + 1) = '\0';
    }

    std::string logPath = std::string(dllDir) + "vrinject.log";
    vrinject::Logger::Init(logPath);

    LOG_INFO("========================================");
    LOG_INFO("VRInject Framework v0.1 - Initializing");
    LOG_INFO("========================================");
    LOG_INFO("Module base: 0x%p", static_cast<void*>(g_hModule));

    if (vrinject::DX11Hook::Initialize()) {
        LOG_INFO("DX11 hook installation succeeded");
    } else {
        LOG_ERROR("DX11 hook installation FAILED");
    }

    if (vrinject::DX12Hook::Initialize()) {
        LOG_INFO("DX12 hook installation succeeded");
    } else {
        LOG_ERROR("DX12 hook installation FAILED");
    }

    if (vrinject::InputHook::GetInstance().Initialize()) {
        LOG_INFO("XInput hook installation succeeded");
    } else {
        LOG_ERROR("XInput hook installation failed");
    }

    return 0;
}

} // anonymous namespace

// ============================================================================
// DllMain
// ============================================================================
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID /*reserved*/) {
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
                // We don't need the handle; let the OS clean up when the
                // thread exits.
                ::CloseHandle(reinterpret_cast<HANDLE>(hThread));
            }
            break;
        }

        case DLL_PROCESS_DETACH: {
            vrinject::InputHook::GetInstance().Shutdown();
            LOG_INFO("VRInject Framework - Shutting down");
            vrinject::DX11Hook::Shutdown();
            vrinject::DX12Hook::Shutdown();
            LOG_INFO("Shutdown complete – goodbye.");
            vrinject::Logger::Shutdown();
            break;
        }

        default:
            break;
    }

    return TRUE;
}
