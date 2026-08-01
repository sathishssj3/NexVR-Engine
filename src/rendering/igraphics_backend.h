#pragma once

#include "../core/camera_snapshot.h"
#include "../core/depth_snapshot.h"
#include "../core/graphics_types.h"
#include "stereo_renderer.h"

namespace vrinject {

// Forward declarations
struct StereoParams;

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
        const CameraSnapshot& camSnapshot, 
        const DepthSnapshot& depthSnapshot, 
        const StereoParams& params
    ) = 0;
    
    virtual void* GetLeftEyeTexture() = 0;
    virtual void* GetRightEyeTexture() = 0;
    
    // Returns the underlying API identity (DX11, DX12, Vulkan)
    virtual GraphicsBackend GetAPI() const = 0;
};

} // namespace vrinject
