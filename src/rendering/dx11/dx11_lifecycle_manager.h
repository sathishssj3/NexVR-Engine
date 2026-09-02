#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <atomic>
#include <mutex>
#include "core/diagnostic_context.h"
#include "heuristics/render_frame_snapshot.h"

namespace vrinject {

// Dx11ResourceEpoch replaced by GraphicsResourceEpoch

struct Dx11DeviceResources {
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediateContext;
};

struct Dx11SwapchainResources {
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
    
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT width = 0;
    UINT height = 0;
    bool fullscreen = false;
};

// Dx11ContextSnapshot replaced by RenderFrameSnapshot

class Dx11LifecycleManager {
public:
    static Dx11LifecycleManager& Get() {
        static Dx11LifecycleManager instance;
        return instance;
    }

    // Called on Present to handle transitions and return an immutable snapshot for the frame
    RenderFrameSnapshot ProcessPresent(IDXGISwapChain* swapChain, HRESULT presentResult);
    
    // Forces shutdown/cleanup when DLL detaches or game closes
    void Shutdown();

    // Resets state back to UNINITIALIZED for testing
    void Reset();

    // Called by ResizeBuffers hook BEFORE the original call.
    // Releases all ComPtr references to swapchain buffers so ResizeBuffers can succeed.
    void ReleaseSwapchainReferences();
    
    // Called by ResizeBuffers hook AFTER the original call succeeds.
    // Marks state for rebuild so backbuffer is re-acquired on next Present.
    void NotifyResizeComplete();

    RenderState GetState() const { return m_state.load(std::memory_order_relaxed); }
    GraphicsResourceEpoch GetEpoch() const { return m_epoch; }

private:
    Dx11LifecycleManager() = default;
    
    void Discover(IDXGISwapChain* swapChain);
    void Validate(IDXGISwapChain* swapChain);
    bool Rebuild();
    void HandleDeviceLoss(HRESULT presentResult);
    bool Recover();
    void Degrade(const char* reason);
    
    RenderFrameSnapshot CreateSnapshot();
    
    std::mutex m_mutex;
    
    std::atomic<RenderState> m_state{RenderState::UNINITIALIZED};
    GraphicsResourceEpoch m_epoch;
    int m_recoveryAttempts = 0;
    const int MAX_RECOVERY_ATTEMPTS = 3;
    
    Dx11DeviceResources m_deviceResources;
    Dx11SwapchainResources m_swapchainResources;
};

} // namespace vrinject
