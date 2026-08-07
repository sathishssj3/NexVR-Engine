#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <memory>
#include <vector>

// Forward declarations for ONNX Runtime to avoid polluting all headers
namespace Ort {
    class Env;
    class Session;
    class MemoryInfo;
    struct SessionOptions;
    class Value;
}

namespace vrinject {

class NeuralInpainter {
public:
    NeuralInpainter();
    ~NeuralInpainter();

    // Initializes ONNX Runtime with DirectML execution provider and allocates D3D11 resources
    bool Initialize(ID3D11Device* device, const std::string& modelPath, UINT fullWidth, UINT fullHeight);
    
    void Shutdown();

    // Executes the Depth-Aware Gated U-Net inference.
    // Downscales warped texture and mask, runs DirectML ONNX inference, 
    // and returns the low-resolution inpainted texture SRV.
    ID3D11ShaderResourceView* Inpaint(ID3D11DeviceContext* ctx, 
                                      ID3D11ShaderResourceView* warpedColorSRV,
                                      ID3D11ShaderResourceView* depthSRV);

    UINT GetLowResWidth() const { return m_lowResWidth; }
    UINT GetLowResHeight() const { return m_lowResHeight; }
    bool IsInitialized() const { return m_initialized; }

private:
    bool CreateD3D11Resources(ID3D11Device* device);
    bool InitializeOnnxSession(const std::string& modelPath);

    // ONNX Runtime Core & DirectML Session
    std::shared_ptr<Ort::Env> m_env;
    std::unique_ptr<Ort::SessionOptions> m_sessionOptions;
    std::unique_ptr<Ort::Session> m_session;
    std::unique_ptr<Ort::MemoryInfo> m_memoryInfo;

    // Direct3D 11 Interop resources for Zero-Copy / GPU tensor inference
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_inputTensorTex;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_outputTensorTex;
    
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_inputTensorSRV;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_inputTensorRTV;
    
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_outputTensorSRV;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_outputTensorRTV;
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
    bool m_isSimulated;

    // CPU staging tensor buffer when interop texture readback is required
    std::vector<float> m_inputTensorBuffer;
    std::vector<float> m_outputTensorBuffer;
};

} // namespace vrinject
