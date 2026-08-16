#include "rendering/dx12/dx12_pipeline_state_cache.h"
#include <cassert>
#include <iostream>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

namespace vrinject {

DX12PipelineStateCache::~DX12PipelineStateCache() {
    Shutdown();
}

bool DX12PipelineStateCache::Initialize(ID3D12Device* device) {
    SetSafeCreationPhase(true);
    
    // Pre-cache root signature and PSO
    GetStereoRootSignature(device);
    GetStereoPSO(device);
    
    SetSafeCreationPhase(false);
    
    // m_stereoPSO might be null until we have the real HLSL shader
    return m_stereoRootSignature != nullptr;
}

void DX12PipelineStateCache::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stereoPSO.Reset();
    m_stereoRootSignature.Reset();
}

void DX12PipelineStateCache::SetSafeCreationPhase(bool isSafe) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_isSafeCreationPhase = isSafe;
}

ID3D12RootSignature* DX12PipelineStateCache::GetStereoRootSignature(ID3D12Device* device) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_stereoRootSignature) {
        return m_stereoRootSignature.Get();
    }
    
    // STRICT CONSTRAINT ENFORCEMENT
    // Zero per-frame Root Signature creation. Must be initialized during safe phases.
    if (!m_isSafeCreationPhase) {
        std::cerr << "CRITICAL ERROR: Attempting to create RootSignature outside of Initialize/Resize phase!" << std::endl;
        assert(false && "Zero per-frame root signature creation constraint violated!");
        return nullptr;
    }
    
    // Root Signature definition for compute (CBV, SRV, UAV)
    D3D12_DESCRIPTOR_RANGE ranges[3] = {};
    // CBV
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    ranges[0].NumDescriptors = 1;
    ranges[0].BaseShaderRegister = 0;
    ranges[0].RegisterSpace = 0;
    ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    
    // SRV
    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[1].NumDescriptors = 2; // Camera, Depth
    ranges[1].BaseShaderRegister = 0;
    ranges[1].RegisterSpace = 0;
    ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // UAV
    ranges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[2].NumDescriptors = 2; // Left Eye, Right Eye
    ranges[2].BaseShaderRegister = 0;
    ranges[2].RegisterSpace = 0;
    ranges[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[3] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &ranges[0];
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &ranges[1];
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[2].DescriptorTable.pDescriptorRanges = &ranges[2];
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    
    // Static sampler for reprojection mapping
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 0;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = 3;
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumStaticSamplers = 1;
    rootSignatureDesc.pStaticSamplers = &sampler;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    Microsoft::WRL::ComPtr<ID3DBlob> signature;
    Microsoft::WRL::ComPtr<ID3DBlob> error;
    if (FAILED(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error))) {
        if (error) {
            std::cerr << "Root Signature Serialization Error: " << (char*)error->GetBufferPointer() << std::endl;
        }
        return nullptr;
    }

    if (FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_stereoRootSignature)))) {
        return nullptr;
    }

    return m_stereoRootSignature.Get();
}

ID3D12PipelineState* DX12PipelineStateCache::GetStereoPSO(ID3D12Device* device) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Return cached PSO if available
    if (m_stereoPSO) {
        return m_stereoPSO.Get();
    }
    
    // PSO doesn't exist yet — this is expected until the real HLSL shader
    // is compiled and a .cso is provided. Return nullptr silently;
    // the renderer handles this by skipping the dispatch.
    return nullptr;
}

bool DX12PipelineStateCache::CompileShader(const std::wstring& filename, const std::string& entrypoint, const std::string& target, std::vector<uint8_t>& outBytecode) {
    // Optional fallback if CSO doesn't exist
    return false;
}

} // namespace vrinject
