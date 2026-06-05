#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

namespace vrinject {

// Per-frame captured resources from the hooked game
struct FrameResources {
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* swapChain = nullptr;
    
    // Captured textures (framework-owned copies)
    Microsoft::WRL::ComPtr<ID3D11Texture2D> colorBuffer;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthBuffer;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> colorSRV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depthSRV;
    
    UINT width = 0;
    UINT height = 0;
    DXGI_FORMAT depthFormat = DXGI_FORMAT_UNKNOWN;
    bool reversedZ = false;
    bool valid = false;
};

namespace DX11Hook {
    // Initializes MinHook, creates dummy D3D11 device, extracts vtables, and hooks Present/OMSetRenderTargets
    bool Initialize();
    
    // Cleans up hooks and releases COM resources
    void Shutdown();
    
    // Access the latest captured frame resources
    FrameResources GetCurrentFrame();
    
    // Callbacks for pipeline integration
    using OnFrameCallback = void(*)(const FrameResources&);
    void SetOnFrameCallback(OnFrameCallback callback);
}

} // namespace vrinject
