#pragma once
#include <d3d11.h>
#include "core/stereo_types.h"
#include "rendering/stereo/stereo_resource_manager.h"
#include "rendering/asw_manager.h"
#include "vr/cutscene_detector.h"
#include "vr/comfort_vignette.h"
#include "vr/horizon_lock.h"

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
    // --- Chunk 0: bytes 0–255 (existing, unchanged layout) ---
    Matrix4x4 inverseViewProj;
    Matrix4x4 leftViewProj;
    Matrix4x4 rightViewProj;
    
    Vector3 originalEyePos;
    float contrast;
    
    Vector3 leftEyePos;
    float saturation;
    
    Vector3 rightEyePos;
    float brightness;
    
    uint32_t width;
    uint32_t height;
    uint32_t shouldAttemptStereo;
    uint32_t srgbCorrection;

    // --- Chunk 1: bytes 256–511 (P0.2/P1.1/P1.2/P2 features) ---
    // P0.2: Floating Curved HUD
    uint32_t curvedHudEnabled;      // bool → uint (0 or 1)
    float hudDistance;               // Virtual HUD distance in meters
    float hudCurvature;              // Cylinder curvature factor [0, 1]
    float _pad0;

    // P1.2: Dynamic Comfort Vignette
    float comfortVignetteRadius;     // Normalized radius [0, 1] where vignette starts
    float comfortVignetteFeather;    // Feather width [0, 1]
    // P1.1: Cutscene Auto-Theater Mode
    float theaterModeWeight;         // Smooth blend weight [0, 1] (0 = normal, 1 = cinema)
    float theaterDistance;           // Virtual cinema screen distance

    // P2: Horizon Lock
    float horizonRollCorrection;     // Roll angle correction in radians
    float horizonLockStrength;       // Blend factor [0, 1]
    float _pad1;
    float _pad2;

    // Remaining padding to reach exactly 512 bytes
    // Chunk 0 = 256 bytes. Chunk 1 = 12 floats × 4 = 48 bytes. Need 256 − 48 = 208 bytes = 52 floats padding.
    float _reserved[52];
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
    CutsceneDetector cutsceneDetector_;
    ComfortVignetteCalculator vignetteCalculator_;
    HorizonLock horizonLock_;
};

} // namespace vrinject
