#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

namespace vrinject {
namespace DXGIFactoryHook {

    bool Initialize();
    void Shutdown();

    // Returns the captured command queue if it matches the swapchain's device
    ID3D12CommandQueue* GetCapturedCommandQueue();

}
}
