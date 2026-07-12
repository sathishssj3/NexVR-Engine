// dxgi_proxy.cpp – DXGI proxy DLL entry point
//
// This file compiles as dxgi.dll and forwards all real DXGI exports to the
// system dxgi.dll located in the Windows System32 directory.
//
// Previously the exports were forwarded with hardcoded #pragma comment(linker)
// directives pointing at "C:\Windows\System32\dxgi.xxx". This breaks on:
//   - Non-C: Windows installs  (e.g. D: or E: drives)
//   - ARM64 / IA-64 Windows layouts
//   - Windows installations with non-standard %SystemRoot% paths
//
// FIX: We now resolve the system dxgi.dll path at runtime using
// GetSystemDirectoryA, then forward calls through GetProcAddress.
// This works on every Windows machine regardless of drive or locale.

#include <windows.h>

// ---------------------------------------------------------------------------
// Real system dxgi.dll handle
// ---------------------------------------------------------------------------
static HMODULE g_realDxgi = nullptr;

// Resolve and cache the real system dxgi.dll.
// Called once from DllMain under DLL_PROCESS_ATTACH.
static bool LoadRealDxgi() {
    if (g_realDxgi) return true;

    // GetSystemDirectoryA returns the correct System32 path on all machines:
    //   - x64 native process  → C:\Windows\System32
    //   - WoW64 (x86 on x64) → C:\Windows\SysWOW64  (redirected automatically)
    //   - Custom %SystemRoot% → respected automatically
    char sysDir[MAX_PATH] = {};
    UINT len = GetSystemDirectoryA(sysDir, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return false;

    char realPath[MAX_PATH] = {};
    // strncat_s to avoid buffer overflow
    strncpy_s(realPath, sysDir, _TRUNCATE);
    strncat_s(realPath, "\\dxgi.dll", _TRUNCATE);

    g_realDxgi = LoadLibraryA(realPath);
    return (g_realDxgi != nullptr);
}

// ---------------------------------------------------------------------------
// Helper: get a proc from the real dxgi.dll
// ---------------------------------------------------------------------------
static FARPROC GetRealProc(const char* name) {
    if (!g_realDxgi) return nullptr;
    return GetProcAddress(g_realDxgi, name);
}

// ---------------------------------------------------------------------------
// Forwarded DXGI exports
// Each shim looks up the real function on first call (lazy, thread-safe via
// the OS loader guarantee that DllMain has already run).
// ---------------------------------------------------------------------------

extern "C" {

// ApplyCOMPats
__declspec(dllexport) void WINAPI ApplyCOMPats() {
    static auto fn = (void(WINAPI*)())GetRealProc("ApplyCOMPats");
    if (fn) fn();
}

// CreateDXGIFactory
__declspec(dllexport) HRESULT WINAPI CreateDXGIFactory(REFIID riid, void** ppFactory) {
    static auto fn = (HRESULT(WINAPI*)(REFIID, void**))GetRealProc("CreateDXGIFactory");
    return fn ? fn(riid, ppFactory) : E_NOTIMPL;
}

// CreateDXGIFactory1
__declspec(dllexport) HRESULT WINAPI CreateDXGIFactory1(REFIID riid, void** ppFactory) {
    static auto fn = (HRESULT(WINAPI*)(REFIID, void**))GetRealProc("CreateDXGIFactory1");
    return fn ? fn(riid, ppFactory) : E_NOTIMPL;
}

// CreateDXGIFactory2
__declspec(dllexport) HRESULT WINAPI CreateDXGIFactory2(UINT Flags, REFIID riid, void** ppFactory) {
    static auto fn = (HRESULT(WINAPI*)(UINT, REFIID, void**))GetRealProc("CreateDXGIFactory2");
    return fn ? fn(Flags, riid, ppFactory) : E_NOTIMPL;
}

// DXGID3D10CreateDevice
__declspec(dllexport) HRESULT WINAPI DXGID3D10CreateDevice(
    HMODULE d3d10core, void* pFactory, void* pAdapter,
    UINT Flags, void* pUnknown, void** ppDevice)
{
    typedef HRESULT(WINAPI* Fn)(HMODULE, void*, void*, UINT, void*, void**);
    static auto fn = (Fn)GetRealProc("DXGID3D10CreateDevice");
    return fn ? fn(d3d10core, pFactory, pAdapter, Flags, pUnknown, ppDevice) : E_NOTIMPL;
}

// DXGID3D10CreateLayeredDevice
__declspec(dllexport) HRESULT WINAPI DXGID3D10CreateLayeredDevice(void* pUnknown) {
    typedef HRESULT(WINAPI* Fn)(void*);
    static auto fn = (Fn)GetRealProc("DXGID3D10CreateLayeredDevice");
    return fn ? fn(pUnknown) : E_NOTIMPL;
}

// DXGID3D10GetLayeredDeviceSize
__declspec(dllexport) SIZE_T WINAPI DXGID3D10GetLayeredDeviceSize(const void* pLayers, UINT NumLayers) {
    typedef SIZE_T(WINAPI* Fn)(const void*, UINT);
    static auto fn = (Fn)GetRealProc("DXGID3D10GetLayeredDeviceSize");
    return fn ? fn(pLayers, NumLayers) : 0;
}

// DXGID3D10RegisterLayers
__declspec(dllexport) HRESULT WINAPI DXGID3D10RegisterLayers(const void* pLayers, UINT NumLayers) {
    typedef HRESULT(WINAPI* Fn)(const void*, UINT);
    static auto fn = (Fn)GetRealProc("DXGID3D10RegisterLayers");
    return fn ? fn(pLayers, NumLayers) : E_NOTIMPL;
}

// DXGIDeclareAdapterRemovalSupport
__declspec(dllexport) HRESULT WINAPI DXGIDeclareAdapterRemovalSupport() {
    static auto fn = (HRESULT(WINAPI*)())GetRealProc("DXGIDeclareAdapterRemovalSupport");
    return fn ? fn() : E_NOTIMPL;
}

// DXGIGetDebugInterface1
__declspec(dllexport) HRESULT WINAPI DXGIGetDebugInterface1(UINT Flags, REFIID riid, void** pDebug) {
    static auto fn = (HRESULT(WINAPI*)(UINT, REFIID, void**))GetRealProc("DXGIGetDebugInterface1");
    return fn ? fn(Flags, riid, pDebug) : E_NOTIMPL;
}

// DXGIReportAdapterConfiguration
__declspec(dllexport) void WINAPI DXGIReportAdapterConfiguration() {
    static auto fn = (void(WINAPI*)())GetRealProc("DXGIReportAdapterConfiguration");
    if (fn) fn();
}

} // extern "C"

// ---------------------------------------------------------------------------
// DllMain
// ---------------------------------------------------------------------------
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID /*lpReserved*/) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);

        // Step 1: Load the real system dxgi.dll using the runtime system path.
        if (!LoadRealDxgi()) {
            // This should never happen on a valid Windows installation.
            MessageBoxA(nullptr,
                "VRInject proxy: Failed to load system dxgi.dll.\n"
                "Ensure Windows is installed correctly.",
                "VRInject Error", MB_OK | MB_ICONERROR);
            return FALSE;
        }

        // Step 2: Load vrinject.dll from the same directory as this proxy.
        // Using hModule ensures we find it next to THIS DLL, not by name search.
        char szPath[MAX_PATH] = {};
        GetModuleFileNameA(hModule, szPath, MAX_PATH);
        char* lastSlash = strrchr(szPath, '\\');
        if (lastSlash) {
            *(lastSlash + 1) = '\0';
            strncat_s(szPath, "vrinject.dll", _TRUNCATE);
            LoadLibraryA(szPath);
        }
    } else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        if (g_realDxgi) {
            FreeLibrary(g_realDxgi);
            g_realDxgi = nullptr;
        }
    }
    return TRUE;
}
