#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <atomic>
#include <mutex>
#include "core/diagnostic_context.h"
#include "heuristics/render_frame_snapshot.h"

namespace vrinject {

struct Dx12DeviceResources {
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
};

struct Dx12SwapchainResources {
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
    Microsoft::WRL::ComPtr<ID3D12Resource> backBuffer;
    
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT width = 0;
    UINT height = 0;
    bool fullscreen = false;
};

class Dx12LifecycleManager {
public:
    static Dx12LifecycleManager& Get() {
        static Dx12LifecycleManager instance;
        return instance;
    }

    // Called on Present to handle transitions and return an immutable snapshot for the frame
    RenderFrameSnapshot ProcessPresent(IDXGISwapChain* swapChain, ID3D12CommandQueue* commandQueue, HRESULT presentResult);
    
    // Releases references to swapchain backbuffer prior to ResizeBuffers
    void ReleaseSwapchainReferences();

    // Marks swapchain for rebuild after ResizeBuffers completes
    void NotifyResizeComplete();

    // Forces shutdown/cleanup when DLL detaches or game closes
    void Shutdown();

    // Resets state back to UNINITIALIZED for testing
    void Reset();

    RenderState GetState() const { return m_state.load(std::memory_order_relaxed); }
    GraphicsResourceEpoch GetEpoch() const { return m_epoch; }
    
    ID3D12CommandQueue* GetMainQueue() const { return m_deviceResources.commandQueue.Get(); }
    ID3D12Device* GetDevice() const { return m_deviceResources.device.Get(); }
    ID3D12Resource* GetBackBuffer() const { return m_swapchainResources.backBuffer.Get(); }
    DXGI_FORMAT GetFormat() const { return m_swapchainResources.format; }
    UINT GetWidth() const { return m_swapchainResources.width; }
    UINT GetHeight() const { return m_swapchainResources.height; }
    
    void SetDeviceResourcesForTest(ID3D12Device* device, ID3D12CommandQueue* queue) {
        m_deviceResources.device = device;
        m_deviceResources.commandQueue = queue;
        m_state.store(RenderState::RUNNING);
    }

private:
    Dx12LifecycleManager() = default;
    
    void Discover(IDXGISwapChain* swapChain, ID3D12CommandQueue* commandQueue);
    void Validate(IDXGISwapChain* swapChain, ID3D12CommandQueue* commandQueue);
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
    
    Dx12DeviceResources m_deviceResources;
    Dx12SwapchainResources m_swapchainResources;
};

} // namespace vrinject
