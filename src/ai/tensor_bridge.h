#pragma once

#include <d3d11.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <onnxruntime_cxx_api.h>

namespace NexVR {
namespace AI {

// Bridges DirectX GPU textures with ONNX Runtime DirectML Tensors
// to prevent costly VRAM -> System RAM -> VRAM roundtrips.
class TensorBridge {
public:
    TensorBridge();
    ~TensorBridge();

    // Prevent copying
    TensorBridge(const TensorBridge&) = delete;
    TensorBridge& operator=(const TensorBridge&) = delete;

    // Bind a D3D11 Texture2D as an input tensor for ONNX inference
    Ort::Value BindInputTextureDX11(const char* inputName, Microsoft::WRL::ComPtr<ID3D11Texture2D> texture, const std::vector<int64_t>& shape);
    
    // Bind a D3D12 Resource as an input tensor for ONNX inference
    Ort::Value BindInputResourceDX12(const char* inputName, Microsoft::WRL::ComPtr<ID3D12Resource> resource, const std::vector<int64_t>& shape);

    // Retrieve DML execution context handle for synchronization
    void* GetDmlExecutionContext() const;

private:
    // Opaque pointers to ONNX runtime / DML objects
    void* m_dmlDevice;
    void* m_dmlCommandQueue;
};

} // namespace AI
} // namespace NexVR
