#pragma once
#include <d3d11.h>
#include <wrl/client.h>
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
    float    padding[3]      = {0, 0, 0}; // Pad to 16 byte multiple (48 bytes total)
};

class StereoPipeline {
public:
    bool Initialize(ID3D11Device* device, UINT width, UINT height, 
                    const std::string& moduleDir);
    void Render(ID3D11DeviceContext* deferredCtx,
                ID3D11DeviceContext* immediateCtx,
                ID3D11ShaderResourceView* colorSRV,
                ID3D11ShaderResourceView* depthSRV,
                StereoParams& params,
                float deltaTime);
    void RenderSideBySide(ID3D11DeviceContext* ctx,
                          ID3D11RenderTargetView* backbufferRTV);
    void Shutdown();

    ID3D11ShaderResourceView* GetLeftEyeSRV()  const { return m_leftEyeSRV.Get(); }
    ID3D11ShaderResourceView* GetRightEyeSRV() const { return m_rightEyeSRV.Get(); }
    ID3D11DeviceContext* GetDeferredContext() const { return m_deferredContext.Get(); }
    const ComfortGuard& GetComfortGuard() const { return m_comfortGuard; }
    void ReadAndLogProfiling(ID3D11DeviceContext* immediateCtx);
    OpenXRManager* GetOpenXRManager() { return &m_openxrManager; }

private:
    bool LoadShader(ID3D11Device* device, const std::string& path, 
                    ID3D11ComputeShader** outCS);
    bool CreateResources(ID3D11Device* device, UINT width, UINT height);

    UINT m_width = 0;
    UINT m_height = 0;

    // Compute shaders
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_warpCS;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_resolveCS;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_fillCS;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_blurCS;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_blendCS;

    // Right eye output (left eye = original frame)
    Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_rightEyeTex;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_rightEyeUAV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  m_rightEyeSRV;
    
    // Temporary texture for bilateral blur pass
    Microsoft::WRL::ComPtr<ID3D11Texture2D>           m_blurTempTex;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_blurTempUAV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  m_blurTempSRV;
    
    // Left eye reference (copy of original)
    Microsoft::WRL::ComPtr<ID3D11Texture2D>           m_leftEyeTex;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  m_leftEyeSRV;

    // Atomic depth-index buffer for warp pass (uint per pixel)
    Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_warpBufferTex;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_warpBufferUAV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  m_warpBufferSRV;

    // Side-by-side output for debug display
    Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_sideBySideTex;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  m_sideBySideSRV;

    // Constant buffer for stereo parameters
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_paramsCB;

    // Full-screen quad for side-by-side rendering
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_fullscreenVS;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  m_fullscreenPS;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_sampler;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_deferredContext;

    // Profiling queries
    Microsoft::WRL::ComPtr<ID3D11Query> m_disjointQuery;
    Microsoft::WRL::ComPtr<ID3D11Query> m_startQuery;
    Microsoft::WRL::ComPtr<ID3D11Query> m_warpEndQuery;
    Microsoft::WRL::ComPtr<ID3D11Query> m_resolveEndQuery;
    Microsoft::WRL::ComPtr<ID3D11Query> m_fillEndQuery;
    Microsoft::WRL::ComPtr<ID3D11Query> m_blurEndQuery;

    ComfortGuard m_comfortGuard;
    NeuralInpainter m_neuralInpainter;
    OpenXRManager m_openxrManager;
};

} // namespace vrinject
