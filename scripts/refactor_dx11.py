import re
import sys

def main():
    file_path = r"c:\Users\sathi\.gemini\antigravity\scratch\vr-inject\src\hooks\dx11_hook.cpp"
    with open(file_path, "r") as f:
        content = f.read()

    # 1. Add Present1_t and OriginalPresent1
    content = content.replace(
        "typedef HRESULT(__stdcall* Present_t)(IDXGISwapChain*, UINT, UINT);",
        "typedef HRESULT(__stdcall* Present_t)(IDXGISwapChain*, UINT, UINT);\ntypedef HRESULT(__stdcall* Present1_t)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);"
    )
    content = content.replace(
        "Present_t OriginalPresent = nullptr;",
        "Present_t OriginalPresent = nullptr;\nPresent1_t OriginalPresent1 = nullptr;"
    )

    # 2. Refactor hkPresent into a template ProcessPresent
    # Find the start of hkPresent
    start_idx = content.find("HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {")
    end_idx = content.find("void __stdcall hkOMSetRenderTargets", start_idx)
    
    hk_present_body = content[start_idx:end_idx]

    # Modify the signature and returns
    new_body = hk_present_body.replace(
        "HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {",
        "template<typename OriginalFunc, typename... Args>\nHRESULT ProcessPresent(IDXGISwapChain* pSwapChain, OriginalFunc originalFunc, Args... args) {"
    )
    new_body = new_body.replace(
        "OriginalPresent(pSwapChain, SyncInterval, Flags)",
        "originalFunc(pSwapChain, args...)"
    )

    # Now add the actual hkPresent and hkPresent1
    hooks = """
HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    return ProcessPresent(pSwapChain, OriginalPresent, SyncInterval, Flags);
}

HRESULT __stdcall hkPresent1(IDXGISwapChain1* pSwapChain, UINT SyncInterval, UINT PresentFlags, const DXGI_PRESENT_PARAMETERS* pPresentParameters) {
    return ProcessPresent(pSwapChain, OriginalPresent1, SyncInterval, PresentFlags, pPresentParameters);
}

"""
    
    content = content[:start_idx] + new_body + hooks + content[end_idx:]

    # 3. Add hook for Present1 in Initialize
    init_replace = """    void* presentAddress = pSwapChainVtable[8];
    void* omSetRenderTargetsAddress = pContextVtable[33];
    void* mapAddress = pContextVtable[14];
    void* updateSubresourceAddress = pContextVtable[16];

    void* present1Address = nullptr;
    IDXGISwapChain1* pSwapChain1 = nullptr;
    if (SUCCEEDED(pSwapChain->QueryInterface(__uuidof(IDXGISwapChain1), (void**)&pSwapChain1))) {
        void** pSwapChain1Vtable = *reinterpret_cast<void***>(pSwapChain1);
        present1Address = pSwapChain1Vtable[22];
        pSwapChain1->Release();
    }"""
    content = content.replace(
        "    void* presentAddress = pSwapChainVtable[8];\n    void* omSetRenderTargetsAddress = pContextVtable[33];\n    void* mapAddress = pContextVtable[14];\n    void* updateSubresourceAddress = pContextVtable[16];",
        init_replace
    )

    hook_create_replace = """    if (MH_CreateHook(presentAddress, (void*)hkPresent, (void**)&OriginalPresent) != MH_OK) {
        LOG_ERROR("MH_CreateHook failed for Present");
        return false;
    }
    if (present1Address && MH_CreateHook(present1Address, (void*)hkPresent1, (void**)&OriginalPresent1) != MH_OK) {
        LOG_ERROR("MH_CreateHook failed for Present1");
        return false;
    }"""
    content = content.replace(
        "    if (MH_CreateHook(presentAddress, (void*)hkPresent, (void**)&OriginalPresent) != MH_OK) {\n        LOG_ERROR(\"MH_CreateHook failed for Present\");\n        return false;\n    }",
        hook_create_replace
    )

    with open(file_path, "w") as f:
        f.write(content)

if __name__ == "__main__":
    main()
