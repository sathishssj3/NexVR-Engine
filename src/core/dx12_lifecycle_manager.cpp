#include "dx12_lifecycle_manager.h"
#include "logger.h"

namespace vrinject {

RenderFrameSnapshot Dx12LifecycleManager::ProcessPresent(IDXGISwapChain* swapChain, ID3D12CommandQueue* commandQueue, HRESULT presentResult) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // 1. Handle explicit device loss before anything else
    if (presentResult == DXGI_ERROR_DEVICE_REMOVED || 
        presentResult == DXGI_ERROR_DEVICE_RESET || 
        presentResult == DXGI_ERROR_DEVICE_HUNG) {
        HandleDeviceLoss(presentResult);
    }

    RenderState currentState = m_state.load(std::memory_order_relaxed);

    if (currentState == RenderState::DEGRADED || currentState == RenderState::SHUTTING_DOWN || currentState == RenderState::STOPPED) {
        // Safe pass-through mode
        return CreateSnapshot();
    }

    if (currentState == RenderState::UNINITIALIZED || currentState == RenderState::RECOVERING || currentState == RenderState::REINITIALIZING) {
        Discover(swapChain, commandQueue);
    } else if (currentState == RenderState::RUNNING) {
        Validate(swapChain, commandQueue);
    }

    currentState = m_state.load(std::memory_order_relaxed);

    if (currentState == RenderState::RESIZE_PENDING || currentState == RenderState::INITIALIZING) {
        if (Rebuild()) {
            m_state.store(RenderState::RUNNING, std::memory_order_release);
        } else {
            // Failed to rebuild resources, might need another frame or device is lost
            m_state.store(RenderState::RESIZE_PENDING, std::memory_order_release);
        }
    } else if (currentState == RenderState::DEVICE_REMOVED) {
        if (Recover()) {
            m_state.store(RenderState::REINITIALIZING, std::memory_order_release);
        }
    }

    // Refresh active backbuffer resource for this frame
    if (m_state.load(std::memory_order_relaxed) == RenderState::RUNNING && m_swapchainResources.swapChain) {
        Microsoft::WRL::ComPtr<IDXGISwapChain3> sc3;
        if (SUCCEEDED(m_swapchainResources.swapChain.As(&sc3))) {
            UINT currentIdx = sc3->GetCurrentBackBufferIndex();
            m_swapchainResources.swapChain->GetBuffer(currentIdx, IID_PPV_ARGS(&m_swapchainResources.backBuffer));
        }
    }

    return CreateSnapshot();
}

void Dx12LifecycleManager::Discover(IDXGISwapChain* swapChain, ID3D12CommandQueue* commandQueue) {
    if (!swapChain || !commandQueue) return;

    m_swapchainResources.swapChain = swapChain;
    m_deviceResources.commandQueue = commandQueue;

    if (FAILED(commandQueue->GetDevice(__uuidof(ID3D12Device), (void**)&m_deviceResources.device))) {
        Degrade("Failed to query ID3D12Device from CommandQueue");
        return;
    }

    m_epoch.deviceGeneration++;
    m_epoch.swapchainGeneration++;
    m_epoch.resourceGeneration++;
    
    m_state.store(RenderState::INITIALIZING, std::memory_order_release);
    LOG_INFO("Dx12LifecycleManager: Discovered Device %p, Queue %p (Epoch %llu:%llu:%llu)", 
        m_deviceResources.device.Get(), commandQueue, m_epoch.deviceGeneration, m_epoch.swapchainGeneration, m_epoch.resourceGeneration);
}

void Dx12LifecycleManager::Validate(IDXGISwapChain* swapChain, ID3D12CommandQueue* commandQueue) {
    if (!swapChain || !m_swapchainResources.swapChain || !commandQueue || !m_deviceResources.commandQueue) return;

    // Check if swapchain pointer or command queue changed
    if (swapChain != m_swapchainResources.swapChain.Get() || commandQueue != m_deviceResources.commandQueue.Get()) {
        LOG_WARN("Dx12LifecycleManager: Swapchain or CommandQueue pointer changed. Triggering rediscovery.");
        m_state.store(RenderState::REINITIALIZING, std::memory_order_release);
        return;
    }

    // Check for resize/fullscreen changes
    DXGI_SWAP_CHAIN_DESC desc;
    if (SUCCEEDED(swapChain->GetDesc(&desc))) {
        bool needsRebuild = false;
        
        if (desc.BufferDesc.Width != m_swapchainResources.width || desc.BufferDesc.Height != m_swapchainResources.height) {
            LOG_INFO("Dx12LifecycleManager: Resize detected (%u x %u -> %u x %u)", 
                     m_swapchainResources.width, m_swapchainResources.height, desc.BufferDesc.Width, desc.BufferDesc.Height);
            needsRebuild = true;
        }
        
        BOOL fullscreen = FALSE;
        swapChain->GetFullscreenState(&fullscreen, nullptr);
        if ((fullscreen == TRUE) != m_swapchainResources.fullscreen) {
            LOG_INFO("Dx12LifecycleManager: Fullscreen state changed (%d -> %d)", m_swapchainResources.fullscreen, fullscreen);
            needsRebuild = true;
        }

        if (needsRebuild) {
            m_state.store(RenderState::RESIZE_PENDING, std::memory_order_release);
        }
    }
}

bool Dx12LifecycleManager::Rebuild() {
    if (!m_swapchainResources.swapChain || !m_deviceResources.device) return false;

    // Release old resources
    m_swapchainResources.backBuffer.Reset();

    DXGI_SWAP_CHAIN_DESC desc;
    if (FAILED(m_swapchainResources.swapChain->GetDesc(&desc))) {
        return false;
    }

    m_swapchainResources.width = desc.BufferDesc.Width;
    m_swapchainResources.height = desc.BufferDesc.Height;
    m_swapchainResources.format = desc.BufferDesc.Format;
    
    BOOL fullscreen = FALSE;
    m_swapchainResources.swapChain->GetFullscreenState(&fullscreen, nullptr);
    m_swapchainResources.fullscreen = (fullscreen == TRUE);

    if (FAILED(m_swapchainResources.swapChain->GetBuffer(0, __uuidof(ID3D12Resource), (void**)&m_swapchainResources.backBuffer))) {
        LOG_ERROR("Dx12LifecycleManager: Failed to get backbuffer during rebuild");
        return false;
    }

    if (m_state.load() == RenderState::RESIZE_PENDING) {
        m_epoch.swapchainGeneration++; // swapchain internal configuration changed
    }
    m_epoch.resourceGeneration++;
    
    LOG_INFO("Dx12LifecycleManager: Resources rebuilt successfully (Epoch %llu:%llu:%llu)", 
        m_epoch.deviceGeneration, m_epoch.swapchainGeneration, m_epoch.resourceGeneration);
    m_recoveryAttempts = 0; // Reset budget on successful build
    return true;
}

void Dx12LifecycleManager::HandleDeviceLoss(HRESULT presentResult) {
    if (m_state.load(std::memory_order_relaxed) == RenderState::DEGRADED) return;

    const char* reason = "Unknown";
    if (presentResult == DXGI_ERROR_DEVICE_REMOVED) reason = "DXGI_ERROR_DEVICE_REMOVED";
    else if (presentResult == DXGI_ERROR_DEVICE_RESET) reason = "DXGI_ERROR_DEVICE_RESET";
    else if (presentResult == DXGI_ERROR_DEVICE_HUNG) reason = "DXGI_ERROR_DEVICE_HUNG";

    LOG_ERROR("Dx12LifecycleManager: Device Lost Detected: %s", reason);

    if (m_deviceResources.device) {
        HRESULT removeReason = m_deviceResources.device->GetDeviceRemovedReason();
        LOG_ERROR("Dx12LifecycleManager: GetDeviceRemovedReason(): 0x%X", removeReason);
        DiagnosticContext::Get().PostEvent(DiagnosticLevel::Critical, "DX12Lifecycle", "Graphics device lost");
    }

    m_state.store(RenderState::DEVICE_REMOVED, std::memory_order_release);
}

bool Dx12LifecycleManager::Recover() {
    m_recoveryAttempts++;
    LOG_WARN("Dx12LifecycleManager: Attempting recovery %d/%d", m_recoveryAttempts, MAX_RECOVERY_ATTEMPTS);

    if (m_recoveryAttempts > MAX_RECOVERY_ATTEMPTS) {
        Degrade("Exceeded maximum recovery attempts");
        return false;
    }

    // Wipe current state to force a full re-discovery on the next frame
    m_swapchainResources.backBuffer.Reset();
    m_deviceResources.commandQueue.Reset();
    m_deviceResources.device.Reset();
    m_swapchainResources.swapChain.Reset();

    // Typically, the game will create a new device and swapchain, which we will hook on the next Present.
    return true;
}

void Dx12LifecycleManager::Degrade(const char* reason) {
    LOG_ERROR("Dx12LifecycleManager: Degrading NexVR Features. Reason: %s", reason);
    DiagnosticContext::Get().PostEvent(DiagnosticLevel::Warning, "DX12Lifecycle", reason);
    m_state.store(RenderState::DEGRADED, std::memory_order_release);
}

RenderFrameSnapshot Dx12LifecycleManager::CreateSnapshot() {
    RenderFrameSnapshot snap;
    snap.backend = GraphicsBackend::DX12;
    snap.epoch = m_epoch;
    snap.state = m_state.load(std::memory_order_relaxed);
    snap.nativeDevice = m_deviceResources.device.Get();
    snap.nativeContext = m_deviceResources.commandQueue.Get();
    snap.nativeSwapchain = m_swapchainResources.swapChain.Get();
    snap.backBuffer = m_swapchainResources.backBuffer.Get();
    snap.format = m_swapchainResources.format;
    snap.width = m_swapchainResources.width;
    snap.height = m_swapchainResources.height;
    snap.fullscreen = m_swapchainResources.fullscreen;
    return snap;
}

void Dx12LifecycleManager::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state.store(RenderState::SHUTTING_DOWN, std::memory_order_release);
    m_swapchainResources.backBuffer.Reset();
    m_deviceResources.commandQueue.Reset();
    m_deviceResources.device.Reset();
    m_swapchainResources.swapChain.Reset();
    m_state.store(RenderState::STOPPED, std::memory_order_release);
    LOG_INFO("Dx12LifecycleManager: Shutdown complete");
}

void Dx12LifecycleManager::Reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state.store(RenderState::UNINITIALIZED, std::memory_order_release);
    m_epoch = GraphicsResourceEpoch();
    m_recoveryAttempts = 0;
    m_swapchainResources.backBuffer.Reset();
    m_deviceResources.commandQueue.Reset();
    m_deviceResources.device.Reset();
    m_swapchainResources.swapChain.Reset();
}

} // namespace vrinject
