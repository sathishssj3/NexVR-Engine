// dxgi_proxy.cpp – DXGI proxy DLL entry point
//
// This file compiles as dxgi.dll and forwards all real DXGI exports to the
// system dxgi.dll located in the Windows System32 directory.
//
// Hardened against loader-lock violations (M-1) and persistence vector (H-1):
// System DLL resolution and vrinject chain-loading are deferred outside DllMain,
// and vrinject.dll integrity is verified via SHA-256 against EXPECTED_DLL_HASH.

#include <windows.h>
#include <mutex>
#include <vector>
#include <string>
#include <bcrypt.h>
#include "expected_hash.h"

#pragma comment(lib, "bcrypt.lib")

namespace {

static HMODULE g_realDxgi = nullptr;
static HMODULE g_proxyModule = nullptr;
static std::once_flag g_initOnce;

bool VerifyDllIntegrity(const char* path) {
    if (!path) return false;
    DWORD attrs = GetFileAttributesA(path);
    if (attrs == INVALID_FILE_ATTRIBUTES) return false;

    FILE* f = nullptr;
    if (fopen_s(&f, path, "rb") != 0 || !f) return false;
    char mz[2] = {0};
    size_t n = fread(mz, 1, 2, f);
    fclose(f);
    if (n != 2 || mz[0] != 'M' || mz[1] != 'Z') return false;

    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0) != 0) return false;

    DWORD cbData = 0, cbHashObject = 0;
    if (BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&cbHashObject, sizeof(DWORD), &cbData, 0) != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }
    std::vector<BYTE> pbHashObject(cbHashObject);
    DWORD cbHash = 0;
    if (BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&cbHash, sizeof(DWORD), &cbData, 0) != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }
    std::vector<BYTE> pbHash(cbHash);
    if (BCryptCreateHash(hAlg, &hHash, pbHashObject.data(), cbHashObject, NULL, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    bool hashOk = false;
    if (fopen_s(&f, path, "rb") == 0 && f) {
        BYTE buffer[8192];
        size_t readBytes = 0;
        while ((readBytes = fread(buffer, 1, sizeof(buffer), f)) > 0) {
            BCryptHashData(hHash, buffer, (ULONG)readBytes, 0);
        }
        fclose(f);

        if (BCryptFinishHash(hHash, pbHash.data(), cbHash, 0) == 0) {
            std::wstring computedHash;
            wchar_t hex[3];
            for (DWORD i = 0; i < cbHash; i++) {
                swprintf_s(hex, L"%02X", pbHash[i]);
                computedHash += hex;
            }
            if (computedHash == EXPECTED_DLL_HASH) {
                hashOk = true;
            }
        }
    }

    BCRYPT_DESTROY_HASH:
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return hashOk;
}

void EnsureInitialized() {
    std::call_once(g_initOnce, []() {
        // Step 1: Resolve system dxgi.dll from System32
        char sysDir[MAX_PATH] = {};
        UINT len = GetSystemDirectoryA(sysDir, MAX_PATH);
        if (len > 0 && len < MAX_PATH) {
            char realPath[MAX_PATH] = {};
            strncpy_s(realPath, sysDir, _TRUNCATE);
            strncat_s(realPath, "\\dxgi.dll", _TRUNCATE);
            g_realDxgi = LoadLibraryA(realPath);
        }

        // Step 2: Verify and chain-load vrinject.dll
        if (g_proxyModule) {
            char szPath[MAX_PATH] = {};
            GetModuleFileNameA(g_proxyModule, szPath, MAX_PATH);
            char* lastSlash = strrchr(szPath, '\\');
            if (lastSlash) {
                *(lastSlash + 1) = '\0';
                strncat_s(szPath, "vrinject.dll", _TRUNCATE);
                if (VerifyDllIntegrity(szPath)) {
                    LoadLibraryA(szPath);
                }
            }
        }
    });
}

FARPROC GetRealProc(const char* name) {
    EnsureInitialized();
    if (!g_realDxgi) return nullptr;
    return GetProcAddress(g_realDxgi, name);
}

} // anonymous namespace

extern "C" {

// ApplyCOMPats
__declspec(dllexport) void WINAPI ApplyCOMPats() {
    auto fn = (void(WINAPI*)())GetRealProc("ApplyCOMPats");
    if (fn) fn();
}

// CreateDXGIFactory
__declspec(dllexport) HRESULT WINAPI CreateDXGIFactory(REFIID riid, void** ppFactory) {
    auto fn = (HRESULT(WINAPI*)(REFIID, void**))GetRealProc("CreateDXGIFactory");
    return fn ? fn(riid, ppFactory) : E_FAIL;
}

// CreateDXGIFactory1
__declspec(dllexport) HRESULT WINAPI CreateDXGIFactory1(REFIID riid, void** ppFactory) {
    auto fn = (HRESULT(WINAPI*)(REFIID, void**))GetRealProc("CreateDXGIFactory1");
    return fn ? fn(riid, ppFactory) : E_FAIL;
}

// CreateDXGIFactory2
__declspec(dllexport) HRESULT WINAPI CreateDXGIFactory2(UINT Flags, REFIID riid, void** ppFactory) {
    auto fn = (HRESULT(WINAPI*)(UINT, REFIID, void**))GetRealProc("CreateDXGIFactory2");
    return fn ? fn(Flags, riid, ppFactory) : E_FAIL;
}

// DXGID3D10CreateDevice
__declspec(dllexport) HRESULT WINAPI DXGID3D10CreateDevice(
    HMODULE d3d10core, void* pFactory, void* pAdapter,
    UINT Flags, void* pUnknown, void** ppDevice)
{
    typedef HRESULT(WINAPI* Fn)(HMODULE, void*, void*, UINT, void*, void**);
    auto fn = (Fn)GetRealProc("DXGID3D10CreateDevice");
    return fn ? fn(d3d10core, pFactory, pAdapter, Flags, pUnknown, ppDevice) : E_FAIL;
}

// DXGID3D10CreateLayeredDevice
__declspec(dllexport) HRESULT WINAPI DXGID3D10CreateLayeredDevice(void* pUnknown) {
    typedef HRESULT(WINAPI* Fn)(void*);
    auto fn = (Fn)GetRealProc("DXGID3D10CreateLayeredDevice");
    return fn ? fn(pUnknown) : E_FAIL;
}

// DXGID3D10GetLayeredDeviceSize
__declspec(dllexport) SIZE_T WINAPI DXGID3D10GetLayeredDeviceSize(const void* pLayers, UINT NumLayers) {
    typedef SIZE_T(WINAPI* Fn)(const void*, UINT);
    auto fn = (Fn)GetRealProc("DXGID3D10GetLayeredDeviceSize");
    return fn ? fn(pLayers, NumLayers) : 0;
}

// DXGID3D10RegisterLayers
__declspec(dllexport) HRESULT WINAPI DXGID3D10RegisterLayers(const void* pLayers, UINT NumLayers) {
    typedef HRESULT(WINAPI* Fn)(const void*, UINT);
    auto fn = (Fn)GetRealProc("DXGID3D10RegisterLayers");
    return fn ? fn(pLayers, NumLayers) : E_FAIL;
}

// DXGIDeclareAdapterRemovalSupport
__declspec(dllexport) HRESULT WINAPI DXGIDeclareAdapterRemovalSupport() {
    auto fn = (HRESULT(WINAPI*)())GetRealProc("DXGIDeclareAdapterRemovalSupport");
    return fn ? fn() : E_FAIL;
}

// DXGIGetDebugInterface1
__declspec(dllexport) HRESULT WINAPI DXGIGetDebugInterface1(UINT Flags, REFIID riid, void** pDebug) {
    auto fn = (HRESULT(WINAPI*)(UINT, REFIID, void**))GetRealProc("DXGIGetDebugInterface1");
    return fn ? fn(Flags, riid, pDebug) : E_FAIL;
}

// DXGIReportAdapterConfiguration
__declspec(dllexport) void WINAPI DXGIReportAdapterConfiguration() {
    auto fn = (void(WINAPI*)())GetRealProc("DXGIReportAdapterConfiguration");
    if (fn) fn();
}

} // extern "C"

// ---------------------------------------------------------------------------
// DllMain
// ---------------------------------------------------------------------------
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID /*lpReserved*/) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        g_proxyModule = hModule;
    } else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        if (g_realDxgi) {
            FreeLibrary(g_realDxgi);
            g_realDxgi = nullptr;
        }
    }
    return TRUE;
}
