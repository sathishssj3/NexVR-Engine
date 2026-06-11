#include "stereo_pipeline.h"
#include "../core/logger.h"
#include "../core/config_manager.h"
#include <d3dcompiler.h>
#include <vector>

#pragma comment(lib, "d3dcompiler.lib")

namespace vrinject {

bool StereoPipeline::Initialize(ID3D11Device* device, UINT width, UINT height, const std::string& moduleDir) {
    m_width = width;
    m_height = height;

    std::string shaderDir = moduleDir + "\\shaders";
    std::string modelDir = moduleDir + "\\models";

    if (!LoadShader(device, shaderDir + "\\stereo_warp.hlsl", &m_warpCS)) return false;
    if (!LoadShader(device, shaderDir + "\\stereo_resolve.hlsl", &m_resolveCS)) return false;
    if (!LoadShader(device, shaderDir + "\\disocclusion_fill.hlsl", &m_fillCS)) return false;
    if (!LoadShader(device, shaderDir + "\\bilateral_blur.hlsl", &m_blurCS)) return false;
    if (!LoadShader(device, shaderDir + "\\bilateral_blend.hlsl", &m_blendCS)) return false;

    if (FAILED(device->CreateDeferredContext(0, &m_deferredContext))) {
        LOG_ERROR("Failed to create deferred context");
        return false;
    }

    if (!CreateResources(device, width, height)) return false;

    if (!m_openxrManager.Initialize(GraphicsAPI::DX11, device, nullptr, width, height)) {
        LOG_WARN("Failed to initialize OpenXRManager (No VR headset?). Falling back to 2D Side-By-Side monitor display.");
    }

    // Compile side-by-side VS/PS at runtime
    const char* vsCode = R"(
        struct VSOut { float4 pos : SV_Position; float2 tex : TEXCOORD0; };
        VSOut main(uint id : SV_VertexID) {
            VSOut output;
            output.tex = float2((id << 1) & 2, id & 2);
            output.pos = float4(output.tex * float2(2, -2) + float2(-1, 1), 0, 1);
            return output;
        }
    )";
    const char* psCode = R"(
        Texture2D LeftTex : register(t0);
        Texture2D RightTex : register(t1);
        SamplerState smp : register(s0);
        float4 main(float4 pos : SV_Position, float2 tex : TEXCOORD0) : SV_Target {
            if (tex.x < 0.5) return LeftTex.Sample(smp, float2(tex.x * 2.0, tex.y));
            else return RightTex.Sample(smp, float2((tex.x - 0.5) * 2.0, tex.y));
        }
    )";

    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob, errBlob;
    HRESULT hrVS = D3DCompile(vsCode, strlen(vsCode), nullptr, nullptr, nullptr, "main", "vs_5_0", 0, 0, &vsBlob, &errBlob);
    if (FAILED(hrVS) || !vsBlob) {
        LOG_ERROR("VS Compile Failed: %s", errBlob ? (char*)errBlob->GetBufferPointer() : "Unknown error");
        return false;
    }
    
    HRESULT hrPS = D3DCompile(psCode, strlen(psCode), nullptr, nullptr, nullptr, "main", "ps_5_0", 0, 0, &psBlob, &errBlob);
    if (FAILED(hrPS) || !psBlob) {
        LOG_ERROR("PS Compile Failed: %s", errBlob ? (char*)errBlob->GetBufferPointer() : "Unknown error");
        return false;
    }

    device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_fullscreenVS);
    device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_fullscreenPS);

    if (!m_comfortGuard.Initialize(device, shaderDir)) {
        LOG_ERROR("Failed to initialize ComfortGuard");
        return false;
    }

    // Initialize Neural Inpainter with explicit model path
    if (!m_neuralInpainter.Initialize(device, modelDir + "\\inpainter_fp16.onnx", width, height)) {
        LOG_ERROR("Failed to initialize NeuralInpainter");
        // We don't return false here to allow the game to still run with fallback shaders if ORT fails
    }

    return true;
}

bool StereoPipeline::LoadShader(ID3D11Device* device, const std::string& path, ID3D11ComputeShader** outCS) {
    std::wstring wPath(path.begin(), path.end());
    Microsoft::WRL::ComPtr<ID3DBlob> shaderBlob, errBlob;
    
    // Check if it's a precompiled .cso file
    if (path.find(".cso") != std::string::npos) {
        FILE* file = nullptr;
        fopen_s(&file, path.c_str(), "rb");
        if (!file) return false;
        fseek(file, 0, SEEK_END);
        size_t size = ftell(file);
        fseek(file, 0, SEEK_SET);
        std::vector<char> buffer(size);
        fread(buffer.data(), 1, size, file);
        fclose(file);
        return SUCCEEDED(device->CreateComputeShader(buffer.data(), size, nullptr, outCS));
    }

    // Compile from .hlsl source at runtime
    HRESULT hr = D3DCompileFromFile(wPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "CSMain", "cs_5_0", D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &shaderBlob, &errBlob);
    if (FAILED(hr) || !shaderBlob) {
        LOG_ERROR("Failed to compile shader %s: %s", path.c_str(), errBlob ? (char*)errBlob->GetBufferPointer() : "Unknown error");
        return false;
    }
    return SUCCEEDED(device->CreateComputeShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, outCS));
}

bool StereoPipeline::CreateResources(ID3D11Device* device, UINT width, UINT height) {
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

    if (FAILED(device->CreateTexture2D(&texDesc, nullptr, &m_rightEyeTex))) return false;
    device->CreateShaderResourceView(m_rightEyeTex.Get(), nullptr, &m_rightEyeSRV);
    device->CreateUnorderedAccessView(m_rightEyeTex.Get(), nullptr, &m_rightEyeUAV);

    if (FAILED(device->CreateTexture2D(&texDesc, nullptr, &m_blurTempTex))) return false;
    device->CreateShaderResourceView(m_blurTempTex.Get(), nullptr, &m_blurTempSRV);
    device->CreateUnorderedAccessView(m_blurTempTex.Get(), nullptr, &m_blurTempUAV);

    texDesc.Format = DXGI_FORMAT_R32_UINT;
    if (FAILED(device->CreateTexture2D(&texDesc, nullptr, &m_warpBufferTex))) return false;
    device->CreateShaderResourceView(m_warpBufferTex.Get(), nullptr, &m_warpBufferSRV);
    device->CreateUnorderedAccessView(m_warpBufferTex.Get(), nullptr, &m_warpBufferUAV);

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = (sizeof(StereoParams) + 255) & ~255; // Aligned to 256 for DX12 compatibility
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(device->CreateBuffer(&cbDesc, nullptr, &m_paramsCB))) return false;

    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(device->CreateSamplerState(&sampDesc, &m_sampler))) return false;

    D3D11_QUERY_DESC queryDesc = {};
    queryDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
    if (FAILED(device->CreateQuery(&queryDesc, &m_disjointQuery))) return false;

    queryDesc.Query = D3D11_QUERY_TIMESTAMP;
    if (FAILED(device->CreateQuery(&queryDesc, &m_startQuery))) return false;
    if (FAILED(device->CreateQuery(&queryDesc, &m_warpEndQuery))) return false;
    if (FAILED(device->CreateQuery(&queryDesc, &m_resolveEndQuery))) return false;
    if (FAILED(device->CreateQuery(&queryDesc, &m_fillEndQuery))) return false;
    if (FAILED(device->CreateQuery(&queryDesc, &m_blurEndQuery))) return false;

    return true;
}

void StereoPipeline::Render(ID3D11DeviceContext* deferredCtx, ID3D11DeviceContext* immediateCtx, ID3D11ShaderResourceView* colorSRV, ID3D11ShaderResourceView* depthSRV, StereoParams& params, float deltaTime) {
    m_leftEyeSRV = colorSRV;

    XrFrameState frameState = {XR_TYPE_FRAME_STATE};
    bool openxrActive = m_openxrManager.BeginFrame(frameState);

    auto& cfg = ConfigManager::GetInstance().GetConfig();
    params.ipd = cfg.ipd;
    params.convergence = cfg.convergence;

    // ComfortGuard Dynamic Convergence & Depth Control
    if (depthSRV) {
        m_comfortGuard.SetBaseParams(params.convergence, params.depthStrength);
        
        float dynamicConvergence = params.convergence;
        float dynamicDepthStrength = params.depthStrength;
        m_comfortGuard.UpdateParameters(immediateCtx, deltaTime, dynamicConvergence, dynamicDepthStrength);
        
        params.convergence = dynamicConvergence;
        params.depthStrength = dynamicDepthStrength;
    }

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (m_paramsCB && SUCCEEDED(deferredCtx->Map(m_paramsCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        memcpy(mapped.pData, &params, sizeof(StereoParams));
        deferredCtx->Unmap(m_paramsCB.Get(), 0);
    }

    // Schedule next frame's asynchronous depth analysis on the GPU
    if (depthSRV) {
        m_comfortGuard.AnalyzeDepth(deferredCtx, depthSRV, m_paramsCB.Get(), m_width, m_height);
    }

    UINT clearValue[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
    deferredCtx->ClearUnorderedAccessViewUint(m_warpBufferUAV.Get(), clearValue);

    ID3D11Buffer* cbs[] = { m_paramsCB.Get() };
    deferredCtx->CSSetConstantBuffers(0, 1, cbs);

    if (m_disjointQuery) deferredCtx->Begin(m_disjointQuery.Get());
    if (m_startQuery) deferredCtx->End(m_startQuery.Get());

    // Warp Pass
    ID3D11ShaderResourceView* srvs[] = { colorSRV, depthSRV };
    deferredCtx->CSSetShaderResources(0, 2, srvs);
    ID3D11UnorderedAccessView* uavs[] = { m_warpBufferUAV.Get() };
    deferredCtx->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
    deferredCtx->CSSetShader(m_warpCS.Get(), nullptr, 0);
    deferredCtx->Dispatch((m_width + 7) / 8, (m_height + 7) / 8, 1);

    if (m_warpEndQuery) deferredCtx->End(m_warpEndQuery.Get());

    // Unbind resources
    ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
    deferredCtx->CSSetShaderResources(0, 2, nullSRVs);
    ID3D11UnorderedAccessView* nullUAVs[1] = { nullptr };
    deferredCtx->CSSetUnorderedAccessViews(0, 1, nullUAVs, nullptr);

    // Resolve Pass
    srvs[0] = colorSRV;
    srvs[1] = m_warpBufferSRV.Get();
    deferredCtx->CSSetShaderResources(0, 2, srvs);
    uavs[0] = m_rightEyeUAV.Get();
    deferredCtx->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
    deferredCtx->CSSetShader(m_resolveCS.Get(), nullptr, 0);
    deferredCtx->Dispatch((m_width + 7) / 8, (m_height + 7) / 8, 1);

    if (m_resolveEndQuery) deferredCtx->End(m_resolveEndQuery.Get());

    deferredCtx->CSSetShaderResources(0, 2, nullSRVs);
    deferredCtx->CSSetUnorderedAccessViews(0, 1, nullUAVs, nullptr);

    // Neural Inpainting Pass (FP16 ONNX DirectML)
    ID3D11ShaderResourceView* lowResInpaintSRV = nullptr;
    if (cfg.enableNeuralInpainter) {
        lowResInpaintSRV = m_neuralInpainter.Inpaint(deferredCtx, m_rightEyeSRV.Get(), depthSRV);
    }

    if (lowResInpaintSRV && m_blendCS) {
        // Bilateral Blend Pass
        ID3D11ShaderResourceView* blendSRVs[] = { m_rightEyeSRV.Get(), lowResInpaintSRV };
        deferredCtx->CSSetShaderResources(0, 2, blendSRVs);
        
        uavs[0] = m_blurTempUAV.Get();
        deferredCtx->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
        
        deferredCtx->CSSetShader(m_blendCS.Get(), nullptr, 0);
        deferredCtx->Dispatch((m_width + 15) / 16, (m_height + 15) / 16, 1);
        
        deferredCtx->CSSetShaderResources(0, 2, nullSRVs);
        deferredCtx->CSSetUnorderedAccessViews(0, 1, nullUAVs, nullptr);
        deferredCtx->CopyResource(m_rightEyeTex.Get(), m_blurTempTex.Get());
        
        if (m_blurEndQuery) deferredCtx->End(m_blurEndQuery.Get());
    } else {
        // Fallback to Legacy Shaders
        uavs[0] = m_rightEyeUAV.Get();
        deferredCtx->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
        deferredCtx->CSSetShader(m_fillCS.Get(), nullptr, 0);
        deferredCtx->Dispatch((m_width + 15) / 16, (m_height + 15) / 16, 1);

        if (m_fillEndQuery) deferredCtx->End(m_fillEndQuery.Get());
        deferredCtx->CSSetUnorderedAccessViews(0, 1, nullUAVs, nullptr);
        
        srvs[0] = m_rightEyeSRV.Get();
        deferredCtx->CSSetShaderResources(0, 1, srvs);
        uavs[0] = m_blurTempUAV.Get();
        deferredCtx->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
        deferredCtx->CSSetShader(m_blurCS.Get(), nullptr, 0);
        deferredCtx->Dispatch((m_width + 15) / 16, (m_height + 15) / 16, 1);
        
        deferredCtx->CSSetShaderResources(0, 2, nullSRVs);
        deferredCtx->CSSetUnorderedAccessViews(0, 1, nullUAVs, nullptr);
        deferredCtx->CopyResource(m_rightEyeTex.Get(), m_blurTempTex.Get());
        
        if (m_blurEndQuery) deferredCtx->End(m_blurEndQuery.Get());
    }

    if (m_disjointQuery) deferredCtx->End(m_disjointQuery.Get());

    if (openxrActive) {
        Microsoft::WRL::ComPtr<ID3D11Resource> leftEyeRes;
        if (m_leftEyeSRV) m_leftEyeSRV->GetResource(&leftEyeRes);
        
        Microsoft::WRL::ComPtr<ID3D11Texture2D> leftEyeTex;
        if (leftEyeRes) leftEyeRes.As(&leftEyeTex);

        TextureHandle leftHandle = {};
        leftHandle.nativePtr = leftEyeTex.Get();
        TextureHandle rightHandle = {};
        rightHandle.nativePtr = m_rightEyeTex.Get();

        TextureHandle depthHandle = {};
        Microsoft::WRL::ComPtr<ID3D11Resource> depthRes;
        if (depthSRV) {
            depthSRV->GetResource(&depthRes);
            depthHandle.nativePtr = depthRes.Get();
        }

        m_openxrManager.EndFrame(frameState, leftHandle, rightHandle, depthHandle, &params);
    }
}

void StereoPipeline::RenderSideBySide(ID3D11DeviceContext* ctx, ID3D11RenderTargetView* backbufferRTV) {
    ctx->OMSetRenderTargets(1, &backbufferRTV, nullptr);

    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)m_width; vp.Height = (FLOAT)m_height;
    vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0; vp.TopLeftY = 0;
    ctx->RSSetViewports(1, &vp);

    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->IASetInputLayout(nullptr);
    
    ctx->VSSetShader(m_fullscreenVS.Get(), nullptr, 0);
    ctx->PSSetShader(m_fullscreenPS.Get(), nullptr, 0);

    ID3D11ShaderResourceView* srvs[] = { m_leftEyeSRV.Get(), m_rightEyeSRV.Get() };
    ctx->PSSetShaderResources(0, 2, srvs);

    ID3D11SamplerState* samplers[] = { m_sampler.Get() };
    ctx->PSSetSamplers(0, 1, samplers);

    ctx->Draw(3, 0);

    ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
    ctx->PSSetShaderResources(0, 2, nullSRVs);
}

void StereoPipeline::Shutdown() {
    m_warpCS.Reset(); m_resolveCS.Reset(); m_fillCS.Reset(); m_blurCS.Reset(); m_blendCS.Reset();
    m_rightEyeTex.Reset(); m_rightEyeUAV.Reset(); m_rightEyeSRV.Reset();
    m_blurTempTex.Reset(); m_blurTempUAV.Reset(); m_blurTempSRV.Reset();
    m_warpBufferTex.Reset(); m_warpBufferUAV.Reset(); m_warpBufferSRV.Reset();
    m_paramsCB.Reset(); m_fullscreenVS.Reset(); m_fullscreenPS.Reset();
    m_sampler.Reset(); m_deferredContext.Reset();

    m_disjointQuery.Reset(); m_startQuery.Reset(); m_warpEndQuery.Reset(); 
    m_resolveEndQuery.Reset(); m_fillEndQuery.Reset(); m_blurEndQuery.Reset();

    m_comfortGuard.Shutdown();
    m_neuralInpainter.Shutdown();
    m_openxrManager.Shutdown();
}

void StereoPipeline::ReadAndLogProfiling(ID3D11DeviceContext* immediateCtx) {
    if (!m_disjointQuery || !m_startQuery) return;

    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData;
    // Asynchronous check using D3D11_ASYNC_GETDATA_DONOTFLUSH to avoid stalling the CPU thread
    if (immediateCtx->GetData(m_disjointQuery.Get(), &disjointData, sizeof(disjointData), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_FALSE) return;

    if (disjointData.Disjoint) return;

    UINT64 tsStart, tsWarp, tsResolve, tsFill, tsBlur;
    if (immediateCtx->GetData(m_startQuery.Get(), &tsStart, sizeof(tsStart), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_FALSE) return;
    if (immediateCtx->GetData(m_warpEndQuery.Get(), &tsWarp, sizeof(tsWarp), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_FALSE) return;
    if (immediateCtx->GetData(m_resolveEndQuery.Get(), &tsResolve, sizeof(tsResolve), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_FALSE) return;
    if (immediateCtx->GetData(m_fillEndQuery.Get(), &tsFill, sizeof(tsFill), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_FALSE) return;
    if (immediateCtx->GetData(m_blurEndQuery.Get(), &tsBlur, sizeof(tsBlur), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_FALSE) return;

    static int frameCounter = 0;
    if (++frameCounter % 60 == 0) {
        double freq = static_cast<double>(disjointData.Frequency);
        double warpTime = (tsWarp - tsStart) / freq * 1000.0;
        double resolveTime = (tsResolve - tsWarp) / freq * 1000.0;
        double fillTime = (tsFill - tsResolve) / freq * 1000.0;
        double blurTime = (tsBlur - tsFill) / freq * 1000.0;
        double totalTime = (tsBlur - tsStart) / freq * 1000.0;

        LOG_INFO("GPU Profile (ms) | Total: %.3f | Warp: %.3f | Resolve: %.3f | Fill: %.3f | Blur: %.3f | ComfortGuard: %.2fm, %.2fx", 
            totalTime, warpTime, resolveTime, fillTime, blurTime, m_comfortGuard.GetCurrentConvergence(), m_comfortGuard.GetCurrentDepthStrength());
    }
}

} // namespace vrinject
