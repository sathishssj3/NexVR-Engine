#include "rendering/dx12/dx12_resource_state_tracker.h"

namespace vrinject {

void DX12ResourceStateTracker::RegisterResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState) {
    if (!resource) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_resourceStates[resource] = { initialState };
}

void DX12ResourceStateTracker::UnregisterResource(ID3D12Resource* resource) {
    if (!resource) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_resourceStates.erase(resource);
}

void DX12ResourceStateTracker::ForceResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state) {
    if (!resource) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    m_resourceStates[resource].state = state;
}

bool DX12ResourceStateTracker::TransitionResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES newState) {
    if (!resource) return false;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_resourceStates.find(resource);
    D3D12_RESOURCE_STATES currentState = D3D12_RESOURCE_STATE_COMMON;
    
    if (it != m_resourceStates.end()) {
        currentState = it->second.state;
    } else {
        // Assume COMMON if unknown
        m_resourceStates[resource] = { D3D12_RESOURCE_STATE_COMMON };
    }
    
    // Check for redundancy
    if (currentState == newState) {
        return false; // No transition needed
    }
    
    // Add barrier
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = currentState;
    barrier.Transition.StateAfter = newState;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    
    m_pendingBarriers.push_back(barrier);
    m_resourceStates[resource].state = newState;
    
    return true;
}

bool DX12ResourceStateTracker::TransitionSubresource(ID3D12Resource* resource, UINT subresource, D3D12_RESOURCE_STATES newState) {
    // For now, map subresource transitions to the whole resource since we don't have per-subresource tracking implemented yet.
    // In our VR pipeline we only use ALL_SUBRESOURCES anyway.
    return TransitionResource(resource, newState);
}

const std::vector<D3D12_RESOURCE_BARRIER>& DX12ResourceStateTracker::GetPendingBarriers() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pendingBarriers;
}

void DX12ResourceStateTracker::FlushBarriers(ID3D12GraphicsCommandList* commandList) {
    if (!commandList) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_pendingBarriers.empty()) {
        commandList->ResourceBarrier(static_cast<UINT>(m_pendingBarriers.size()), m_pendingBarriers.data());
        m_pendingBarriers.clear();
    }
}

void DX12ResourceStateTracker::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_resourceStates.clear();
    m_pendingBarriers.clear();
}

} // namespace vrinject
