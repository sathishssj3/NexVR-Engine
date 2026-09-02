#include "rendering/dx11/dx11_graphics_backend.h"
#include "heuristics/camera_lock_manager.h"
#include "heuristics/depth_lock_manager.h"
#include "core/subsystem_context.h"
#include "core/performance_profiler.h"
#include "core/gpu_profiler.h"
#include "core/runtime_state_monitor.h"
#include "rendering/stereo/stereo_pipeline.h"
#include "openxr/openxr_runtime_manager.h"
#include "openxr/openxr_swapchain_manager.h"
#include "openxr/openxr_frame_submitter.h"
#include "rendering/dx11/imgui_dx11_integration.h"
#include "core/overlay_manager.h"
#include <wrl/client.h>
#include <dxgi.h>

namespace vrinject {

DX11GraphicsBackend::DX11GraphicsBackend() {
}

DX11GraphicsBackend::~DX11GraphicsBackend() {
    Shutdown();
}

bool DX11GraphicsBackend::Initialize(void* nativeDevice, void* nativeContext) {
    m_device = static_cast<ID3D11Device*>(nativeDevice);
    m_context = static_cast<ID3D11DeviceContext*>(nativeContext);
    m_resourceManager = std::make_unique<StereoResourceManager>(m_device);
    m_stereoRenderer = std::make_unique<StereoRenderer>();
    if (m_device && m_context) {
        ImGuiDX11Integration::GetInstance().Initialize(m_device, m_context);
    }
    return m_stereoRenderer != nullptr;
}

void DX11GraphicsBackend::Shutdown() {
    ImGuiDX11Integration::GetInstance().Shutdown();
    m_stereoRenderer.reset();
    m_resourceManager.reset();
    m_device = nullptr;
    m_context = nullptr;
}

void DX11GraphicsBackend::SetState(StereoRendererState state) {
    if (m_stereoRenderer) m_stereoRenderer->SetState(state);
}

StereoRendererState DX11GraphicsBackend::GetState() const {
    return m_stereoRenderer ? m_stereoRenderer->GetState() : StereoRendererState::UNINITIALIZED;
}

bool DX11GraphicsBackend::Healthy() const {
    return m_device != nullptr && m_context != nullptr;
}

CameraSnapshot DX11GraphicsBackend::GetCamera() {
    return SubsystemContext::Get().GetCameraLockManager()->GetSnapshot();
}

DepthSnapshot DX11GraphicsBackend::GetDepth() {
    return SubsystemContext::Get().GetDepthLockManager()->GetSnapshot();
}

void* DX11GraphicsBackend::GetLeftEyeTexture() {
    return m_resourceManager ? m_resourceManager->GetLeftEyeTexture() : nullptr;
}

void* DX11GraphicsBackend::GetRightEyeTexture() {
    return m_resourceManager ? m_resourceManager->GetRightEyeTexture() : nullptr;
}

void DX11GraphicsBackend::RenderStereo(
    const RenderFrameSnapshot& frameSnapshot,
    const CameraSnapshot& camSnapshot, 
    const DepthSnapshot& depthSnapshot, 
    const StereoParams& params,
    bool shouldAttemptStereo,
    void* uiMaskHandle) 
{
    if (!m_stereoRenderer || !m_context) {
        return;
    }

    uint32_t width = depthSnapshot.identity.width;
    uint32_t height = depthSnapshot.identity.height;
    if (width == 0) width = frameSnapshot.width;
    if (height == 0) height = frameSnapshot.height;
    if (width == 0) width = 1920;
    if (height == 0) height = 1080;

    StereoConstants constants;
    constants.ipd = params.ipd;
    constants.nearPlane = params.nearPlane;
    constants.farPlane = params.farPlane;
    constants.convergence = params.convergence;
    constants.enableASW = params.enableASW;
    constants.aswTargetFps = params.aswTargetFps;

    EyeView leftEye, rightEye;
    StereoCameraGenerator::Generate(frameSnapshot, camSnapshot, constants, leftEye, rightEye);
    
    StereoFrameContext frameCtx = StereoFrameBuilder::Build(
        camSnapshot.frame,
        camSnapshot, 
        depthSnapshot,
        leftEye,
        rightEye,
        constants,
        width,
        height
    );
    
    DXGI_FORMAT targetFmt = static_cast<DXGI_FORMAT>(frameSnapshot.format);
    if (targetFmt == DXGI_FORMAT_UNKNOWN) targetFmt = DXGI_FORMAT_R8G8B8A8_UNORM;

    if (!m_resourceManager->Initialize(width, height, targetFmt)) {
        return;
    }

    // The compute shader samples the game's colour and depth. Those live behind
    // the resource identities the lock managers captured this frame: colour is
    // the swapchain backbuffer stamped in by FrameCoordinator, depth is the
    // locked depth target. Both arrive as raw ID3D11Texture2D*.
    auto* gameColorTex = static_cast<ID3D11Texture2D*>(camSnapshot.resourceIdentity.nativeHandle);
    if (!gameColorTex) {
        gameColorTex = static_cast<ID3D11Texture2D*>(frameSnapshot.backBuffer);
    }
    auto* gameDepthTex = static_cast<ID3D11Texture2D*>(depthSnapshot.identity.nativeHandle);

    // Bail rather than dispatch with nulls.
    if (!m_resourceManager->EnsureSourceViews(m_context, gameColorTex, gameDepthTex)) {
        return;
    }

    m_stereoRenderer->RenderStereoFrame(
        m_context,
        m_resourceManager.get(),
        frameCtx,
        m_resourceManager->GetGameColorSRV(),
        m_resourceManager->GetGameDepthSRV(),
        shouldAttemptStereo
    );
}

bool DX11GraphicsBackend::CreateOpenXRSession(openxr::OpenXRRuntimeManager* xrRuntime, const RenderFrameSnapshot& snapshot) {
    if (!xrRuntime) return false;
    return xrRuntime->CreateSession(static_cast<ID3D11Device*>(snapshot.nativeDevice));
}

void DX11GraphicsBackend::SubmitStereoFrame(
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
    XrPosef leftPose, rightPose;
    XrFovf leftFov, rightFov;

    {
        ScopedCpuTimer oxrTimer(&cpuProfiler, CpuSegment::OpenXrSubmission);
        gpuProfiler.BeginSegment(static_cast<ID3D11DeviceContext*>(currentSnapshot.nativeContext),
                                 GpuSegment::OpenXrSubmission);

        if (oxrSubmitter->BeginAndAcquireDX11(xrRuntime->GetSession(),
                                              xrRuntime->GetReferenceSpace(),
                                              oxrSwapchain,
                                              leftPose, leftFov, rightPose, rightFov)) {
            currentSnapshot.leftPose = leftPose;
            currentSnapshot.leftFov = leftFov;
            currentSnapshot.rightPose = rightPose;
            currentSnapshot.rightFov = rightFov;

            gpuProfiler.EndSegment(static_cast<ID3D11DeviceContext*>(currentSnapshot.nativeContext),
                                   GpuSegment::OpenXrSubmission);

            // Determine which textures to submit to OpenXR
            ID3D11Texture2D* leftSubmit = nullptr;
            ID3D11Texture2D* rightSubmit = nullptr;

            // Render stereo / monoscopic frame through compute pipeline to apply color linearization & alpha
            RenderStereo(currentSnapshot, camSnapshot, depthSnapshot, params, shouldAttemptStereo, uiMaskHandle);
            leftSubmit = static_cast<ID3D11Texture2D*>(GetLeftEyeTexture());
            rightSubmit = static_cast<ID3D11Texture2D*>(GetRightEyeTexture());

            // Monoscopic raw fallback: if compute shader produced nulls
            // (heuristic failed, compute shader error, etc.), copy the game's
            // raw backbuffer directly to both eyes. The player sees a flat 2D
            // view in the headset, but at least it's not black.
            if (!leftSubmit || !rightSubmit) {
                auto* gameBackBuffer = static_cast<ID3D11Texture2D*>(currentSnapshot.backBuffer);
                
                // Last-resort: if backBuffer is null (lifecycle not ready), try
                // grabbing it directly from the swapchain. This handles edge cases
                // where the game just started or the lifecycle manager hasn't rebuilt.
                Microsoft::WRL::ComPtr<ID3D11Texture2D> directBackBuffer;
                if (!gameBackBuffer && currentSnapshot.nativeSwapchain) {
                    auto* swapChain = static_cast<IDXGISwapChain*>(currentSnapshot.nativeSwapchain);
                    if (SUCCEEDED(swapChain->GetBuffer(0, IID_PPV_ARGS(&directBackBuffer)))) {
                        gameBackBuffer = directBackBuffer.Get();
                    }
                }
                
                if (gameBackBuffer) {
                    leftSubmit = gameBackBuffer;
                    rightSubmit = gameBackBuffer;
                }
            }

            gpuProfiler.EndSegment(
                static_cast<ID3D11DeviceContext*>(currentSnapshot.nativeContext),
                GpuSegment::StereoCompute);
            gpuProfiler.BeginSegment(
                static_cast<ID3D11DeviceContext*>(currentSnapshot.nativeContext),
                GpuSegment::TextureCopies);

            stateMonitor.UpdateStereoHealth(leftSubmit != nullptr);

            gpuProfiler.BeginSegment(static_cast<ID3D11DeviceContext*>(currentSnapshot.nativeContext),
                                     GpuSegment::OpenXrSubmission);

            // Render In-Headset ImGui Overlay onto VR eye buffers if active
            if (OverlayManager::GetInstance().IsOverlayVisible()) {
                auto* d3d11Context = static_cast<ID3D11DeviceContext*>(currentSnapshot.nativeContext);
                ImGuiDX11Integration::GetInstance().Initialize(m_device, d3d11Context);
                if (leftSubmit) {
                    ImGuiDX11Integration::GetInstance().RenderToTexture(m_device, d3d11Context, leftSubmit);
                }
                if (rightSubmit && rightSubmit != leftSubmit) {
                    ImGuiDX11Integration::GetInstance().RenderToTexture(m_device, d3d11Context, rightSubmit);
                }
            }

            oxrSubmitter->ReleaseAndEndDX11(
                xrRuntime->GetSession(), xrRuntime->GetReferenceSpace(),
                oxrSwapchain,
                static_cast<ID3D11DeviceContext*>(currentSnapshot.nativeContext),
                leftSubmit,
                rightSubmit);

            gpuProfiler.EndSegment(static_cast<ID3D11DeviceContext*>(currentSnapshot.nativeContext),
                                   GpuSegment::OpenXrSubmission);
            gpuProfiler.EndSegment(static_cast<ID3D11DeviceContext*>(currentSnapshot.nativeContext),
                                   GpuSegment::TextureCopies);
            
            stateMonitor.UpdateOpenXrHealth(true);
        } else {
            gpuProfiler.EndSegment(static_cast<ID3D11DeviceContext*>(currentSnapshot.nativeContext),
                                   GpuSegment::OpenXrSubmission);
            stateMonitor.UpdateOpenXrHealth(false);
        }
    }
}

} // namespace vrinject
