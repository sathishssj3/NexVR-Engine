#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <atomic>
#include <memory>
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
    UINT depthWidth = 0;
    UINT depthHeight = 0;
    DXGI_FORMAT depthFormat = DXGI_FORMAT_UNKNOWN;
    bool reversedZ = false;
    bool valid = false;
    bool initAttempted = false;
};

// Double-buffered frame resources for lock-free access
// Single-producer (render thread), single-consumer (VR thread) pattern
// Memory ordering: 
// - Writer: writes data -> release store to m_readIndex (publishes frame)
// - Reader: acquire load from m_readIndex -> reads data
// m_writeIndex is only accessed by writer, so relaxed is sufficient
class FrameResourceManager {
public:
    FrameResourceManager() {
        m_buffers[0] = std::make_unique<FrameResources>();
        m_buffers[1] = std::make_unique<FrameResources>();
        m_writeIndex.store(0, std::memory_order_relaxed);
        m_readIndex.store(0, std::memory_order_relaxed);
    }

    // Called from Present hook (render thread) - writes new frame data
    FrameResources* BeginWrite() {
        // Relaxed load is fine - only writer accesses writeIndex
        uint32_t writeIdx = m_writeIndex.load(std::memory_order_relaxed);
        return m_buffers[writeIdx].get();
    }

    void EndWrite() {
        // Relaxed load is fine - only writer accesses writeIndex
        uint32_t writeIdx = m_writeIndex.load(std::memory_order_relaxed);
        // Release store: publishes the frame to the reader
        m_readIndex.store(writeIdx, std::memory_order_release);
        // Switch to other buffer for next write (relaxed - writer-only)
        m_writeIndex.store(1 - writeIdx, std::memory_order_relaxed);
    }

    // Called from VR thread - reads latest complete frame
    const FrameResources* GetLatestFrame() const {
        // Acquire load: synchronizes with writer's release store
        uint32_t readIdx = m_readIndex.load(std::memory_order_acquire);
        return m_buffers[readIdx].get();
    }

    // Non-blocking try-get for render thread
    bool TryGetLatestFrame(FrameResources& out) const {
        // Acquire load: synchronizes with writer's release store
        uint32_t readIdx = m_readIndex.load(std::memory_order_acquire);
        const FrameResources* src = m_buffers[readIdx].get();
        if (!src->valid) return false;
        
        // Copy only the data we need (shallow copy of ComPtrs is fine - they're ref-counted)
        out = *src;
        return true;
    }

    void Invalidate() {
        m_buffers[0]->valid = false;
        m_buffers[1]->valid = false;
    }

private:
    std::unique_ptr<FrameResources> m_buffers[2];
    std::atomic<uint32_t> m_writeIndex;
    std::atomic<uint32_t> m_readIndex;
};

namespace DX11Hook {
    // Initializes MinHook, creates dummy D3D11 device, extracts vtables, and hooks Present/OMSetRenderTargets
    bool Initialize();

    // Cleans up hooks and releases COM resources
    void Shutdown();

    // Access the latest captured frame resources (lock-free)
    FrameResourceManager& GetFrameManager();
    
    // Legacy compatibility - returns copy of latest frame
    FrameResources GetCurrentFrame();

    // Callbacks for pipeline integration
    using OnFrameCallback = void(*)(const FrameResources&);
    void SetOnFrameCallback(OnFrameCallback callback);
}

} // namespace vrinject