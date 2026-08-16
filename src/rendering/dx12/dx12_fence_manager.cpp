#include "rendering/dx12/dx12_fence_manager.h"
#include <iostream>

namespace vrinject {

DX12FenceManager::~DX12FenceManager() {
    Shutdown();
}

bool DX12FenceManager::Initialize(ID3D12Device* device) {
    if (!device) return false;

    if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)))) {
        return false;
    }
    m_fenceValue = 0;

    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent) {
        return false;
    }

    return true;
}

void DX12FenceManager::Shutdown() {
    // Wait for everything to complete before shutting down
    if (m_fenceEvent) {
        WaitForFenceValue(m_fenceValue);
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }
    
    // Clear out resources
    std::lock_guard<std::mutex> lock(m_mutex);
    m_deferredReleases.clear();
    m_fence.Reset();
}

uint64_t DX12FenceManager::Signal(ID3D12CommandQueue* commandQueue) {
    if (!commandQueue || !m_fence) return 0;
    
    uint64_t signalValue = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_fenceValue++;
        signalValue = m_fenceValue;
    }
    
    commandQueue->Signal(m_fence.Get(), signalValue);
    return signalValue;
}

void DX12FenceManager::WaitForFenceValue(uint64_t fenceValue) {
    if (!m_fence || !m_fenceEvent) return;

    if (m_fence->GetCompletedValue() < fenceValue) {
        m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent);
        DWORD waitResult = ::WaitForSingleObject(m_fenceEvent, 5000);
        if (waitResult == WAIT_TIMEOUT) {
            std::cerr << "CRITICAL ERROR: GPU Fence Wait Timed Out! Device might be hung or removed." << std::endl;
            if (m_fence->GetCompletedValue() == UINT64_MAX) {
                std::cerr << "Device was REMOVED." << std::endl;
            }
            std::terminate(); // Fail the test explicitly
        }
    }
}

void DX12FenceManager::DeferRelease(Microsoft::WRL::ComPtr<IUnknown> resource, uint64_t fenceValue) {
    if (!resource) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    m_deferredReleases.push_back({resource, fenceValue});
}

void DX12FenceManager::ProcessDeferredReleases() {
    if (!m_fence) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    uint64_t completedValue = m_fence->GetCompletedValue();
    
    auto it = m_deferredReleases.begin();
    while (it != m_deferredReleases.end()) {
        if (it->fenceValue <= completedValue) {
            // Releasing the ComPtr reference removes it.
            // Erase from vector
            it = m_deferredReleases.erase(it);
        } else {
            ++it;
        }
    }
}

uint64_t DX12FenceManager::GetCompletedValue() const {
    if (!m_fence) return 0;
    return m_fence->GetCompletedValue();
}

uint64_t DX12FenceManager::GetLastSignaledValue() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_fenceValue;
}

} // namespace vrinject
