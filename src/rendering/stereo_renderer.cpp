#include "stereo_renderer.h"
#include "../core/logger.h"
#include <cstring>

namespace vrinject {

StereoRenderer::StereoRenderer() {
}

bool StereoRenderer::UpdateConstantBuffer(ID3D11DeviceContext* context, 
                                          ID3D11Buffer* cb,
                                          const StereoFrameContext& frameCtx) {
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = context->Map(cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) return false;

    StereoShaderConstants* constants = static_cast<StereoShaderConstants*>(mapped.pData);
    
    // Note: HLSL expects column-major matrices by default, but we can declare them row_major in the shader
    // or transpose them here. Let's assume the shader will declare `row_major float4x4`.
    constants->inverseViewProj = frameCtx.inverseViewProjection;
    constants->leftViewProj = frameCtx.leftEye.viewProjection;
    constants->rightViewProj = frameCtx.rightEye.viewProjection;
    
    constants->originalEyePos = frameCtx.camera.position;
    constants->leftEyePos = frameCtx.leftEye.eyePosition;
    constants->rightEyePos = frameCtx.rightEye.eyePosition;
    
    constants->width = frameCtx.viewport.width;
    constants->height = frameCtx.viewport.height;

    context->Unmap(cb, 0);
    return true;
}

bool StereoRenderer::RenderStereoFrame(ID3D11DeviceContext* context,
                                       StereoResourceManager* resourceManager,
                                       const StereoFrameContext& frameCtx,
                                       ID3D11ShaderResourceView* gameColorSRV,
                                       ID3D11ShaderResourceView* gameDepthSRV) {
    if (state_ != StereoRendererState::READY && state_ != StereoRendererState::RENDERING) {
        return false;
    }

    if (!context || !resourceManager || !gameColorSRV || !gameDepthSRV) {
        state_ = StereoRendererState::FAILED;
        return false;
    }

    state_ = StereoRendererState::RENDERING;

    // 1. Update Constant Buffer
    ID3D11Buffer* cb = resourceManager->GetConstantBuffer();
    if (!UpdateConstantBuffer(context, cb, frameCtx)) {
        return false;
    }

    // 2. Bind State
    ID3D11ComputeShader* cs = resourceManager->GetReprojectionShader();
    context->CSSetShader(cs, nullptr, 0);
    context->CSSetConstantBuffers(0, 1, &cb);

    ID3D11ShaderResourceView* srvs[] = { gameColorSRV, gameDepthSRV };
    context->CSSetShaderResources(0, 2, srvs);

    ID3D11UnorderedAccessView* uavs[] = { resourceManager->GetLeftEyeUAV(), resourceManager->GetRightEyeUAV() };
    context->CSSetUnorderedAccessViews(0, 2, uavs, nullptr);

    // 3. Dispatch
    // Assuming thread group size of 8x8
    uint32_t dispatchX = (frameCtx.viewport.width + 7) / 8;
    uint32_t dispatchY = (frameCtx.viewport.height + 7) / 8;
    context->Dispatch(dispatchX, dispatchY, 1);

    // 4. Unbind
    ID3D11ShaderResourceView* nullSRVs[] = { nullptr, nullptr };
    context->CSSetShaderResources(0, 2, nullSRVs);

    ID3D11UnorderedAccessView* nullUAVs[] = { nullptr, nullptr };
    context->CSSetUnorderedAccessViews(0, 2, nullUAVs, nullptr);
    
    return true;
}

} // namespace vrinject
