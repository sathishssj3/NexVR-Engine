#include "depth_reprojector.h"
#include "shaders/stereo_shaders.h"
#include "../core/logger.h"
#include "stereo_pipeline.h"

namespace vrinject {

bool DepthReprojector::Initialize(ID3D11Device* device) {
    HRESULT hr = device->CreateComputeShader(g_depth_reprojection_DX11, sizeof(g_depth_reprojection_DX11), nullptr, &m_computeShader);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create depth reprojection compute shader");
        return false;
    }

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(ReprojectionConstants);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    
    hr = device->CreateBuffer(&cbDesc, nullptr, &m_constantBuffer);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create depth reprojection constant buffer");
        return false;
    }

    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    hr = device->CreateSamplerState(&sampDesc, &m_samplerState);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create sampler state");
        return false;
    }

    return true;
}

void DepthReprojector::Reproject(ID3D11DeviceContext* context, 
                                 ID3D11ShaderResourceView* leftEyeColor,
                                 ID3D11ShaderResourceView* leftEyeDepth,
                                 ID3D11UnorderedAccessView* rightEyeOutputUAV,
                                 const StereoParams& params) {
    if (!m_computeShader || !m_constantBuffer || !leftEyeColor || !leftEyeDepth || !rightEyeOutputUAV) {
        return;
    }

    // Update constant buffer
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        ReprojectionConstants* constants = static_cast<ReprojectionConstants*>(mapped.pData);
        constants->IPD = params.ipd;
        constants->Convergence = params.convergence;
        constants->NearPlane = params.nearPlane;
        constants->FarPlane = params.farPlane;
        constants->Width = (float)params.width;
        constants->Height = (float)params.height;
        constants->ReversedZ = params.reversedZ;
        constants->Padding = 0.0f;
        context->Unmap(m_constantBuffer.Get(), 0);
    }

    // Set compute shader state
    context->CSSetShader(m_computeShader.Get(), nullptr, 0);
    
    ID3D11Buffer* cbs[] = { m_constantBuffer.Get() };
    context->CSSetConstantBuffers(0, 1, cbs);

    ID3D11ShaderResourceView* srvs[] = { leftEyeColor, leftEyeDepth };
    context->CSSetShaderResources(0, 2, srvs);

    ID3D11UnorderedAccessView* uavs[] = { rightEyeOutputUAV };
    context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

    ID3D11SamplerState* samplers[] = { m_samplerState.Get() };
    context->CSSetSamplers(0, 1, samplers);

    // Dispatch
    UINT threadGroupCountX = (params.width + 15) / 16;
    UINT threadGroupCountY = (params.height + 15) / 16;
    context->Dispatch(threadGroupCountX, threadGroupCountY, 1);

    // Cleanup state
    ID3D11ShaderResourceView* nullSRVs[] = { nullptr, nullptr };
    context->CSSetShaderResources(0, 2, nullSRVs);

    ID3D11UnorderedAccessView* nullUAVs[] = { nullptr };
    context->CSSetUnorderedAccessViews(0, 1, nullUAVs, nullptr);
}

} // namespace vrinject
