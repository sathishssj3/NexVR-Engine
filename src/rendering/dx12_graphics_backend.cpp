#include "dx12_graphics_backend.h"
#include "../core/camera_lock_manager.h"
#include "../core/depth_lock_manager.h"
#include "../core/dx12_lifecycle_manager.h"
#include <iostream>

namespace vrinject {

DX12GraphicsBackend::DX12GraphicsBackend() {
    m_resourceManager = std::make_unique<DX12StereoResourceManager>();
    m_psoCache = std::make_unique<DX12PipelineStateCache>();
    m_stateTracker = std::make_unique<DX12ResourceStateTracker>();
    m_fenceManager = std::make_unique<DX12FenceManager>();
    m_renderer = std::make_unique<DX12StereoRenderer>();
}

DX12GraphicsBackend::~DX12GraphicsBackend() {
    Shutdown();
}

bool DX12GraphicsBackend::Initialize(void* nativeDevice, void* nativeContext) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!nativeDevice) {
        m_state = StereoRendererState::FAILED;
        return false;
    }

    m_device = reinterpret_cast<ID3D12Device*>(nativeDevice);
    
    // We get the command queue from the lifecycle manager
    m_commandQueue = reinterpret_cast<ID3D12CommandQueue*>(Dx12LifecycleManager::Get().GetMainQueue());
    if (!m_commandQueue) {
        std::cerr << "DX12GraphicsBackend: Failed to acquire CommandQueue from LifecycleManager!" << std::endl;
        m_state = StereoRendererState::FAILED;
        return false;
    }

    if (!m_fenceManager->Initialize(m_device.Get())) {
        m_state = StereoRendererState::FAILED;
        return false;
    }

    if (!m_psoCache->Initialize(m_device.Get())) {
        m_state = StereoRendererState::FAILED;
        return false;
    }

    if (!m_resourceManager->Initialize(m_device.Get(), m_fenceManager.get(), m_psoCache.get())) {
        m_state = StereoRendererState::FAILED;
        return false;
    }

    if (!m_renderer->Initialize(m_device.Get())) {
        m_state = StereoRendererState::FAILED;
        return false;
    }

    m_state = StereoRendererState::READY;
    return true;
}

void DX12GraphicsBackend::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_renderer) m_renderer->Shutdown();
    if (m_resourceManager) m_resourceManager->Shutdown();
    if (m_psoCache) m_psoCache->Shutdown();
    if (m_fenceManager) m_fenceManager->Shutdown();
    if (m_stateTracker) m_stateTracker->Clear();

    m_commandQueue.Reset();
    m_device.Reset();
    m_state = StereoRendererState::UNINITIALIZED;
}

void DX12GraphicsBackend::SetState(StereoRendererState state) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state = state;
}

StereoRendererState DX12GraphicsBackend::GetState() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

bool DX12GraphicsBackend::Healthy() const {
    return GetState() == StereoRendererState::READY;
}

CameraSnapshot DX12GraphicsBackend::GetCamera() {
    // Rely on core logic, which has DX12 hooked versions updating it
    return CameraLockManager::Get().GetSnapshot();
}

DepthSnapshot DX12GraphicsBackend::GetDepth() {
    return DepthLockManager::Get().GetSnapshot();
}

void DX12GraphicsBackend::RenderStereo(const CameraSnapshot& camSnapshot, const DepthSnapshot& depthSnapshot, const StereoParams& params) {
    if (m_state != StereoRendererState::READY) return;
    if (!m_commandQueue) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    // Free resources that completed execution on GPU
    m_fenceManager->ProcessDeferredReleases();

    // Update resources based on new snapshots (like handling resizes)
    m_resourceManager->UpdateFrameResources(m_device.Get(), camSnapshot, depthSnapshot);

    // Perform rendering dispatch and synchronize
    bool success = m_renderer->RenderStereo(
        m_commandQueue.Get(), 
        m_resourceManager.get(), 
        m_psoCache.get(), 
        m_stateTracker.get(), 
        m_fenceManager.get(), 
        camSnapshot, 
        depthSnapshot,
        m_oxrLeftDest,
        m_oxrRightDest);
        
    if (!success) {
        m_state = StereoRendererState::DEGRADED;
    }
}

void* DX12GraphicsBackend::GetLeftEyeTexture() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_resourceManager ? m_resourceManager->GetLeftEyeTexture() : nullptr;
}

void* DX12GraphicsBackend::GetRightEyeTexture() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_resourceManager ? m_resourceManager->GetRightEyeTexture() : nullptr;
}

void DX12GraphicsBackend::SetOpenXRSwapchainImages(ID3D12Resource* left, ID3D12Resource* right) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_oxrLeftDest = left;
    m_oxrRightDest = right;
}

} // namespace vrinject
