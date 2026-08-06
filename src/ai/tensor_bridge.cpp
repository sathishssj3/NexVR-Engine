#include "tensor_bridge.h"
#include <iostream>

namespace NexVR {
namespace AI {

TensorBridge::TensorBridge() : m_dmlDevice(nullptr), m_dmlCommandQueue(nullptr) {
    std::cout << "[NexVR AI] Initializing TensorBridge...\n";
}

TensorBridge::~TensorBridge() {
    // Release DML resources
}

Ort::Value TensorBridge::BindInputTextureDX11(const char* inputName, Microsoft::WRL::ComPtr<ID3D11Texture2D> texture, const std::vector<int64_t>& shape) {
    if (!texture) throw std::runtime_error("Invalid DX11 texture");
    // Since ORT DML requires D3D12 resources, binding a DX11 texture zero-copy 
    // requires a shared handle or cross-adapter mapping.
    std::cout << "[NexVR AI] Binding DX11 Texture to ONNX input: " << inputName << "\n";
    
    // Stub return (we will test D3D12 directly for Task 0)
    Ort::MemoryInfo dmlMemoryInfo("DML", OrtDeviceAllocator, 0, OrtMemTypeDefault);
    return Ort::Value::CreateTensor<float>(dmlMemoryInfo, nullptr, 0, shape.data(), shape.size());
}

Ort::Value TensorBridge::BindInputResourceDX12(const char* inputName, Microsoft::WRL::ComPtr<ID3D12Resource> resource, const std::vector<int64_t>& shape) {
    if (!resource) throw std::runtime_error("Invalid DX12 resource");
    std::cout << "[NexVR AI] Binding DX12 Resource to ONNX input: " << inputName << "\n";
    
    // To bind an ID3D12Resource, we create a DML MemoryInfo and pass the resource pointer.
    Ort::MemoryInfo dmlMemoryInfo("DML", OrtDeviceAllocator, 0, OrtMemTypeDefault);
    
    // Compute total elements
    int64_t totalElements = 1;
    for (int64_t dim : shape) totalElements *= dim;
    
    size_t byteSize = totalElements * sizeof(float);
    
    // ORT DML EP interprets pData as the ID3D12Resource pointer
    return Ort::Value::CreateTensor(dmlMemoryInfo, resource.Get(), byteSize, shape.data(), shape.size(), ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT);
}

void* TensorBridge::GetDmlExecutionContext() const {
    return m_dmlCommandQueue;
}

} // namespace AI
} // namespace NexVR
