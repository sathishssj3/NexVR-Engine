#pragma once
#include "irenderer.h"
#include <string>
#include <cstdint>
#include "comfort_guard.h"
#include "neural_inpainter.h"
#include "openxr_manager.h"

namespace vrinject {

struct StereoParams {
    float    ipd             = 0.063f;    // Interpupillary distance (meters)
    float    focalLength     = 1.0f;      // Derived from projection matrix
    float    nearPlane       = 0.1f;      // Near clip plane
    float    farPlane        = 1000.0f;   // Far clip plane
    float    convergence     = 5.0f;      // Distance where parallax = 0
    float    depthStrength   = 1.0f;      // Parallax multiplier (0 = flat, 2 = exaggerated)
    uint32_t width           = 1920;      // Render target width
    uint32_t height          = 1080;      // Render target height
    uint32_t reversedZ       = 0;         // Game uses reversed-Z depth buffer
    uint32_t isNativeStereo  = 0;         // If 1, bypass warp and use side-by-side backbuffer
    float    padding[2]      = {0, 0};    // Pad to 16 byte multiple (48 bytes total)
};

class StereoPipeline {
public:
    bool Initialize(IRenderer* renderer, UINT width, UINT height, 
                    const std::string& moduleDir);
    void RenderComputeOnly(TextureHandle colorSRV,
                           TextureHandle depthSRV,
                           const StereoParams& params);
    void Shutdown();

    TextureHandle GetLeftEyeTexture()  const { return m_leftEyeTex; }
    TextureHandle GetRightEyeTexture() const { return m_rightEyeTex; }
    const ComfortGuard& GetComfortGuard() const { return m_comfortGuard; }
    OpenXRManager* GetOpenXRManager() { return &m_openxrManager; }

private:
    bool LoadShader(const std::string& path, ShaderHandle& outHandle);
    bool CreateResources(UINT width, UINT height);

    IRenderer* m_renderer = nullptr;
    UINT m_width = 0;
    UINT m_height = 0;

    // Compute shaders
    ShaderHandle m_warpCS;
    ShaderHandle m_resolveCS;
    ShaderHandle m_fillCS;
    ShaderHandle m_blurCS;
    ShaderHandle m_blendCS;

    // Right eye output (left eye = original frame)
    TextureHandle m_rightEyeTex;
    
    // Temporary texture for bilateral blur pass
    TextureHandle m_blurTempTex;
    
    // Left eye reference (copy of original)
    TextureHandle m_leftEyeTex;

    // Atomic depth-index buffer for warp pass (uint per pixel)
    TextureHandle m_warpBufferTex;

    ComfortGuard m_comfortGuard;
    NeuralInpainter m_neuralInpainter;
    OpenXRManager m_openxrManager;
};

} // namespace vrinject
