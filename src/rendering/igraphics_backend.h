#pragma once

#include "heuristics/camera_snapshot.h"
#include "heuristics/depth_snapshot.h"
#include "core/graphics_types.h"
#include "heuristics/render_frame_snapshot.h"
#include "rendering/stereo/stereo_renderer.h"

namespace vrinject {

// Forward declarations
struct StereoParams;
class RuntimeStateMonitor;
class PerformanceProfiler;
class GpuProfiler;

namespace openxr {
    class OpenXRRuntimeManager;
    class OpenXRSwapchainManager;
    class OpenXRFrameSubmitter;
}

// Unified abstract interface for any graphics backend (DX11, DX12, Vulkan)
class IGraphicsBackend {
public:
    virtual ~IGraphicsBackend() = default;

    // Initialization
    virtual bool Initialize(void* nativeDevice, void* nativeContext) = 0;
    virtual void Shutdown() = 0;

    virtual void SetState(StereoRendererState state) = 0;
    virtual StereoRendererState GetState() const = 0;

    // Health monitoring
    virtual bool Healthy() const = 0;

    // Camera & Depth queries (Backend implementation-specific)
    // The backend provides the most recent valid snapshots
    virtual CameraSnapshot GetCamera() = 0;
    virtual DepthSnapshot GetDepth() = 0;

    // Main rendering entry point for stereo generation
    virtual void RenderStereo(
        const RenderFrameSnapshot& frameSnapshot,
        const CameraSnapshot& camSnapshot, 
        const DepthSnapshot& depthSnapshot, 
        const StereoParams& params,
        void* uiMaskHandle = nullptr
    ) = 0;
    
    virtual void* GetLeftEyeTexture() = 0;
    virtual void* GetRightEyeTexture() = 0;
    
    // Returns the underlying API identity (DX11, DX12, Vulkan)
    virtual GraphicsBackend GetAPI() const = 0;

    // Create the OpenXR Session using backend-specific device and context
    virtual bool CreateOpenXRSession(openxr::OpenXRRuntimeManager* xrRuntime, const RenderFrameSnapshot& snapshot) = 0;
    
    // Orchestrate OpenXR frame acquisition, stereoscopic rendering, and submission 
    virtual void SubmitStereoFrame(
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
        void* uiMaskHandle = nullptr
    ) = 0;
};

} // namespace vrinject
