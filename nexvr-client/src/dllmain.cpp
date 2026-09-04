// ============================================================================
// dllmain.cpp – VRInject DLL entry point
//
// All initialization is deferred to the explicit RuntimeState machine.
// DllMain is exclusively an OS shim to avoid Loader Lock deadlocks.
// ============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include "core/runtime_state.h"

// Global module handle – needed to resolve our own DLL path at runtime.
HMODULE g_hModule = nullptr;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH: {
            ::DisableThreadLibraryCalls(hModule);
            g_hModule = hModule;
            
            // Defer all work to the RuntimeState machine to prevent loader lock deadlocks
            vrinject::RuntimeState::Get().OnDllProcessAttach(hModule);
            break;
        }

        case DLL_PROCESS_DETACH: {
            // Initiate teardown on a background thread without blocking
            vrinject::RuntimeState::Get().OnDllProcessDetach();
            break;
        }

        default:
            break;
    }

    return TRUE;
}
