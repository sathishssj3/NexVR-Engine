#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <memory>

// Forward declare ONNX Runtime types to avoid polluting header
namespace Ort {
    class Env;
    class Session;
    class SessionOptions;
    class MemoryInfo;
}

namespace vrinject {

class NeuralInpainter {
public:
    NeuralInpainter();
    ~NeuralInpainter();

    // Initializes ONNX Runtime with DirectML execution provider
    bool Initialize(ID3D11Device* device, const std::string& modelPath, UINT fullWidth, UINT fullHeight);
    
    void Shutdown();

    // Executes the U-Net inference.
    // Downscales warped texture and mask, runs ONNX inference, 
    // and returns the low-resolution inpainted texture SRV.
    ID3D11ShaderResourceView* Inpaint(ID3D11DeviceContext* ctx, 
                                      ID3D11ShaderResourceView* warpedColorSRV,
                                      ID3D11ShaderResourceView* depthSRV);

    UINT GetLowResWidth() const { return m_lowResWidth; }
    UINT GetLowResHeight() const { return m_lowResHeight; }

private:
    bool CreateD3D11Resources(ID3D11Device* device);

    // ONNX Runtime Core (Stubbed out)

    // Direct3D 11 Interop resources for Zero-Copy inference
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_inputTensorTex;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_outputTensorTex;
    
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_inputTensorSRV;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_inputTensorRTV;
    
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_outputTensorSRV;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_outputTensorUAV;

    // Downscaling / Processing Shaders
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_downscaleVS;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_downscalePS;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_sampler;

    UINT m_fullWidth;
    UINT m_fullHeight;
    UINT m_lowResWidth;
    UINT m_lowResHeight;

    bool m_initialized;
};

} // namespace vrinject
