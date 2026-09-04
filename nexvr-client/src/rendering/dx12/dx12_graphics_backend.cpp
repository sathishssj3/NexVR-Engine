#include "rendering/dx12/dx12_graphics_backend.h"
#include "heuristics/camera_lock_manager.h"
#include "heuristics/depth_lock_manager.h"
#include "core/performance_profiler.h"
#include "core/subsystem_context.h"
#include "core/gpu_profiler.h"
#include "core/runtime_state_monitor.h"
#include "rendering/stereo/stereo_pipeline.h"
#include "openxr/openxr_runtime_manager.h"
#include "openxr/openxr_swapchain_manager.h"
#include "openxr/openxr_frame_submitter.h"
#include "core/logger.h"
#include "rendering/dx12/dx12_lifecycle_manager.h"
#include "rendering/stereo/stereo_camera_generator.h"
#include "rendering/stereo/stereo_pipeline.h"
#include "rendering/dx12/imgui_dx12_integration.h"
#include "core/overlay_manager.h"
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
    return SubsystemContext::Get().GetCameraLockManager()->GetSnapshot();
}

DepthSnapshot DX12GraphicsBackend::GetDepth() {
    return SubsystemContext::Get().GetDepthLockManager()->GetSnapshot();
}

void DX12GraphicsBackend::RenderStereo(
    const RenderFrameSnapshot& frameSnapshot,
    const CameraSnapshot& camSnapshot, 
    const DepthSnapshot& depthSnapshot, 
    const StereoParams& params,
    bool shouldAttemptStereo,
    void* uiMaskHandle) 
{
    if (m_state != StereoRendererState::READY) return;
    if (!m_commandQueue) return;

    StereoConstants constants;
    constants.ipd = params.ipd;
    constants.nearPlane = params.nearPlane;
    constants.farPlane = params.farPlane;
    constants.convergence = params.convergence;
    constants.enableASW = params.enableASW;
    constants.aswTargetFps = params.aswTargetFps;

    EyeView leftEye, rightEye;
    StereoCameraGenerator::Generate(frameSnapshot, camSnapshot, constants, leftEye, rightEye);

    std::lock_guard<std::mutex> lock(m_mutex);

    // Free resources that completed execution on GPU
    m_fenceManager->ProcessDeferredReleases();

    // Update resources based on new snapshots (like handling resizes) and write stereo matrices
    m_resourceManager->UpdateFrameResources(m_device.Get(), leftEye, rightEye, camSnapshot, depthSnapshot, shouldAttemptStereo);

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
        m_oxrRightDest,
        shouldAttemptStereo,
        uiMaskHandle ? (ID3D12Resource*)uiMaskHandle : nullptr);
        
    if (success) {
        m_state = StereoRendererState::READY;
    } else {
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

bool DX12GraphicsBackend::CreateOpenXRSession(openxr::OpenXRRuntimeManager* xrRuntime, const RenderFrameSnapshot& snapshot) {
    if (!xrRuntime) return false;

    // OpenXR REQUIRES xrGetD3D12GraphicsRequirementsKHR before xrCreateSession
    LUID adapterLuid{};
    if (!xrRuntime->CheckDX12GraphicsRequirements(&adapterLuid)) {
        LOG_ERROR("DX12GraphicsBackend: CheckDX12GraphicsRequirements failed!");
    } else {
        LOG_INFO("DX12GraphicsBackend: DX12 graphics requirements satisfied (LUID: %08x:%08x).",
                 adapterLuid.HighPart, adapterLuid.LowPart);
    }
    bool sessionOk = xrRuntime->CreateSessionDX12(
        static_cast<ID3D12Device *>(snapshot.nativeDevice),
        static_cast<ID3D12CommandQueue *>(snapshot.nativeContext));
    if (sessionOk) {
        LOG_INFO("DX12GraphicsBackend: DX12 OpenXR session created successfully!");
    } else {
        LOG_ERROR("DX12GraphicsBackend: DX12 OpenXR session creation FAILED!");
    }
    return sessionOk;
}

void DX12GraphicsBackend::SubmitStereoFrame(
    openxr::OpenXRRuntimeManager* xrRuntime,
    openxr::OpenXRSwapchainManager* oxrSwapchain,
    openxr::OpenXRFrameSubmitter* oxrSubmitter,
    RenderFrameSnapshot& currentSnapshot,
    const CameraSnapshot& camSnapshot,
    const DepthSnapshot& depthSnapshot,
    const StereoParams& params,
    RuntimeStateMonitor& stateMonitor,
    PerformanceProfiler& cpuProfiler,
    GpuProfiler& gpuProfiler,
    bool shouldAttemptStereo,
    void* uiMaskHandle
) {
    ID3D12Resource *leftDest = nullptr;
    ID3D12Resource *rightDest = nullptr;
    XrPosef leftPose, rightPose;
    XrFovf leftFov, rightFov;

    {
        ScopedCpuTimer oxrTimer(&cpuProfiler, CpuSegment::OpenXrSubmission);
        if (oxrSubmitter->BeginAndAcquireDX12(xrRuntime->GetSession(),
                                              xrRuntime->GetReferenceSpace(),
                                              oxrSwapchain,
                                              leftDest, rightDest,
                                              leftPose, leftFov, rightPose, rightFov)) {
            currentSnapshot.leftPose = leftPose;
            currentSnapshot.leftFov = leftFov;
            currentSnapshot.rightPose = rightPose;
            currentSnapshot.rightFov = rightFov;

            SetOpenXRSwapchainImages(leftDest, rightDest);

            RenderStereo(currentSnapshot, camSnapshot, depthSnapshot, params, shouldAttemptStereo, uiMaskHandle);
            bool stereoOk = (m_state == StereoRendererState::READY);

            oxrSubmitter->ReleaseAndEndDX12(
                xrRuntime->GetSession(), xrRuntime->GetReferenceSpace(),
                oxrSwapchain, currentSnapshot.width,
                currentSnapshot.height, currentSnapshot.width,
                currentSnapshot.height);

            stateMonitor.UpdateStereoHealth(stereoOk || (currentSnapshot.backBuffer != nullptr));
            stateMonitor.UpdateOpenXrHealth(true);
        } else {
            stateMonitor.UpdateOpenXrHealth(false);
        }
    }
}

} // namespace vrinject
