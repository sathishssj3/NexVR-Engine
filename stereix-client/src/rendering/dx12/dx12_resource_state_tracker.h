#pragma once

#include <d3d12.h>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace vrinject {

class DX12ResourceStateTracker {
public:
    DX12ResourceStateTracker() = default;
    ~DX12ResourceStateTracker() = default;

    // Registers a resource with an initial state
    void RegisterResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState);

    // Unregisters a resource (e.g. on destruction)
    void UnregisterResource(ID3D12Resource* resource);

    // Forcibly overrides the known state of a resource without emitting a barrier.
    // Useful for resources where state is changed externally (e.g. OpenXR swapchain images).
    void ForceResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state);

    // Requests a transition for a resource. 
    // Returns true if a barrier is needed (state changed), false if redundant.
    // If needed, the transition is added to an internal pending barriers list.
    bool TransitionResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES newState);

    // Forces a transition for a subresource (advanced use).
    bool TransitionSubresource(ID3D12Resource* resource, UINT subresource, D3D12_RESOURCE_STATES newState);

    // Gets the list of pending barriers.
    const std::vector<D3D12_RESOURCE_BARRIER>& GetPendingBarriers() const;

    // Emits pending barriers to the command list and clears the pending list.
    void FlushBarriers(ID3D12GraphicsCommandList* commandList);

    // Clears all tracking data
    void Clear();

private:
    struct ResourceState {
        D3D12_RESOURCE_STATES state;
        // Optionally map subresources if we want per-subresource tracking,
        // but for VR render targets, we typically transition the whole resource.
    };

    std::unordered_map<ID3D12Resource*, ResourceState> m_resourceStates;
    std::vector<D3D12_RESOURCE_BARRIER> m_pendingBarriers;
    mutable std::mutex m_mutex;
};

} // namespace vrinject
