#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <functional>

namespace vrinject {
namespace DX12Hook {

struct FrameResourcesDX12 {
    ID3D12Device* device = nullptr;
    ID3D12CommandQueue* commandQueue = nullptr;
    IDXGISwapChain* swapChain = nullptr;
    UINT width = 0;
    UINT height = 0;
    bool valid = false;
};

typedef std::function<void(const FrameResourcesDX12&)> OnFrameCallbackDX12;

bool Initialize();
void Shutdown();
FrameResourcesDX12 GetCurrentFrame();
void SetOnFrameCallback(OnFrameCallbackDX12 callback);
void OnPresent(IDXGISwapChain* pSwapChain);


} // namespace DX12Hook
} // namespace vrinject
