#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <mutex>
#include <cstdint>

namespace vrinject {

class DX12FenceManager {
public:
    DX12FenceManager() = default;
    ~DX12FenceManager();

    // Initialize the internal ID3D12Fence and event handle
    bool Initialize(ID3D12Device* device);
    
    // Shutdown and clean up deferred resources
    void Shutdown();

    // Emits a signal to the command queue and increments the internal fence value.
    // Returns the fence value that was just queued for signaling.
    uint64_t Signal(ID3D12CommandQueue* commandQueue);

    // Blocks the CPU until the given fence value has been completed by the GPU.
    void WaitForFenceValue(uint64_t fenceValue);

    // Queues a COM object for release once the specified fence value is completed.
    void DeferRelease(Microsoft::WRL::ComPtr<IUnknown> resource, uint64_t fenceValue);

    // Processes the deferred release queue, releasing any resources whose fence has completed.
    void ProcessDeferredReleases();
    
    // Gets the most recently completed fence value.
    uint64_t GetCompletedValue() const;

    // Gets the last signaled fence value (may not be completed yet).
    uint64_t GetLastSignaledValue() const;

private:
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    HANDLE m_fenceEvent = nullptr;
    uint64_t m_fenceValue = 0;

    struct DeferredResource {
        Microsoft::WRL::ComPtr<IUnknown> resource;
        uint64_t fenceValue;
    };

    std::vector<DeferredResource> m_deferredReleases;
    mutable std::mutex m_mutex;
};

} // namespace vrinject
