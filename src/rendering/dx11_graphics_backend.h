#pragma once
#include "igraphics_backend.h"
#include "../core/dx11_lifecycle_manager.h"
#include "stereo_renderer.h"
#include "stereo_resource_manager.h"
#include "../core/stereo_camera_generator.h"
#include "../core/stereo_frame_builder.h"

namespace vrinject {

class DX11GraphicsBackend : public IGraphicsBackend {
public:
    DX11GraphicsBackend();
    ~DX11GraphicsBackend() override;

    bool Initialize(void* nativeDevice, void* nativeContext) override;
    void Shutdown() override;

    void SetState(StereoRendererState state) override;
    StereoRendererState GetState() const override;

    bool Healthy() const override;

    CameraSnapshot GetCamera() override;
    DepthSnapshot GetDepth() override;
    
    void* GetLeftEyeTexture() override;
    void* GetRightEyeTexture() override;

    void RenderStereo(
        const RenderFrameSnapshot& frameSnapshot,
        const CameraSnapshot& camSnapshot, 
        const DepthSnapshot& depthSnapshot, 
        const StereoParams& params) override;
    
    GraphicsBackend GetAPI() const override { return GraphicsBackend::DX11; }

    bool CreateOpenXRSession(openxr::OpenXRRuntimeManager* xrRuntime, const RenderFrameSnapshot& snapshot) override;
    
    void SubmitStereoFrame(
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
        bool shouldAttemptStereo
    ) override;

private:
    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;
    
    std::unique_ptr<StereoResourceManager> m_resourceManager;
    std::unique_ptr<StereoRenderer> m_stereoRenderer;
};

} // namespace vrinject
