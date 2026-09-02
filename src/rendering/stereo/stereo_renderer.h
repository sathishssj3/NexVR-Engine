#pragma once
#include <d3d11.h>
#include "core/stereo_types.h"
#include "rendering/stereo/stereo_resource_manager.h"
#include "rendering/asw_manager.h"

namespace vrinject {

enum class StereoRendererState {
    UNINITIALIZED,
    INITIALIZING,
    READY,
    RENDERING,
    ASW_ACTIVE,
    RESIZE_PENDING,
    REBUILDING,
    FAILED,
    DEGRADED
};

struct StereoShaderConstants {
    Matrix4x4 inverseViewProj;
    Matrix4x4 leftViewProj;
    Matrix4x4 rightViewProj;
    
    Vector3 originalEyePos;
    float pad0;
    
    Vector3 leftEyePos;
    float pad1;
    
    Vector3 rightEyePos;
    float pad2;
    
    uint32_t width;
    uint32_t height;
    uint32_t shouldAttemptStereo;
    float pad3;
};

class StereoRenderer {
public:
    StereoRenderer();
    
    void SetState(StereoRendererState state) { state_ = state; }
    StereoRendererState GetState() const { return state_; }

    // Renders the stereo frame. Returns true if successful.
    bool RenderStereoFrame(ID3D11DeviceContext* context,
                           StereoResourceManager* resourceManager,
                           const StereoFrameContext& frameCtx,
                           ID3D11ShaderResourceView* gameColorSRV,
                           ID3D11ShaderResourceView* gameDepthSRV,
                           bool shouldAttemptStereo);

private:
    bool UpdateConstantBuffer(ID3D11DeviceContext* context, 
                              ID3D11Buffer* cb,
                              const StereoFrameContext& frameCtx,
                              bool shouldAttemptStereo);

    StereoRendererState state_ = StereoRendererState::UNINITIALIZED;
    AswManager aswManager_;
};

} // namespace vrinject
