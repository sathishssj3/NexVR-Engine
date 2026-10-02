import re
import sys

def main():
    # --- dx12_hook.cpp ---
    file_path = r"c:\Users\sathi\.gemini\antigravity\scratch\vr-inject\src\hooks\dx12_hook.cpp"
    with open(file_path, "r") as f:
        content = f.read()

    # Remove DynamicHookSwapChain
    start_idx = content.find("void DynamicHookSwapChain(IDXGISwapChain* pSwapChain) {")
    if start_idx != -1:
        end_idx = content.find("void Shutdown() {", start_idx)
        content = content[:start_idx] + content[end_idx:]
    
    # Remove hkPresentDX12 and hkPresent1DX12 definitions because they are no longer used
    # But keep OnPresent since DX11Hook calls it!
    # Wait, we can just remove them entirely since DX11Hook calls OnPresent directly!
    start_hk = content.find("HRESULT __stdcall hkPresentDX12")
    end_hk = content.find("ExecuteCommandLists_t OriginalExecuteCommandLists = nullptr;", start_hk)
    if start_hk != -1 and end_hk != -1:
        content = content[:start_hk] + content[end_hk:]
    
    with open(file_path, "w") as f:
        f.write(content)

    # --- dx12_hook.h ---
    header_path = r"c:\Users\sathi\.gemini\antigravity\scratch\vr-inject\src\hooks\dx12_hook.h"
    with open(header_path, "r") as f:
        h_content = f.read()
    
    h_content = h_content.replace("void DynamicHookSwapChain(IDXGISwapChain* pSwapChain);", "")
    with open(header_path, "w") as f:
        f.write(h_content)

    # --- dxgi_factory_hook.cpp ---
    factory_path = r"c:\Users\sathi\.gemini\antigravity\scratch\vr-inject\src\hooks\dxgi_factory_hook.cpp"
    with open(factory_path, "r") as f:
        f_content = f.read()

    # Remove calls to DX12Hook::DynamicHookSwapChain
    f_content = f_content.replace("""        if (ppSwapChain && *ppSwapChain) {
            DX12Hook::DynamicHookSwapChain(*ppSwapChain);
        }""", "")

    with open(factory_path, "w") as f:
        f.write(f_content)

if __name__ == "__main__":
    main()
