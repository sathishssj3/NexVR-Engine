#include "dx11_renderer.h"
#include "../../core/logger.h"
#include <d3dcompiler.h>
#include <vector>
#include <string>
#include <bcrypt.h>
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "bcrypt.lib")

#ifndef EXPECTED_SHADER_HASH
#define EXPECTED_SHADER_HASH L""
#endif

namespace {
std::wstring ComputeShaderHashSHA256(const uint8_t* data, size_t size) {
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    std::wstring hashResult = L"";
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0) != 0) return L"";

    DWORD cbData = 0, cbHashObject = 0;
    if (BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&cbHashObject, sizeof(DWORD), &cbData, 0) == 0) {
        std::vector<BYTE> pbHashObject(cbHashObject);
        DWORD cbHash = 0;
        if (BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&cbHash, sizeof(DWORD), &cbData, 0) == 0) {
            std::vector<BYTE> pbHash(cbHash);
            if (BCryptCreateHash(hAlg, &hHash, pbHashObject.data(), cbHashObject, NULL, 0, 0) == 0) {
                BCryptHashData(hHash, (PUCHAR)data, (ULONG)size, 0);
                if (BCryptFinishHash(hHash, pbHash.data(), cbHash, 0) == 0) {
                    wchar_t hex[3];
                    for (DWORD i = 0; i < cbHash; i++) {
                        swprintf_s(hex, L"%02X", pbHash[i]);
                        hashResult += hex;
                    }
                }
                BCryptDestroyHash(hHash);
            }
        }
    }
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return hashResult;
}
}


bool DX11Renderer::Initialize(void* nativeDevice, void* nativeContext) {
    if (!nativeDevice || !nativeContext) return false;
    m_device = static_cast<ID3D11Device*>(nativeDevice);
    m_context = static_cast<ID3D11DeviceContext*>(nativeContext);
    m_device->AddRef();
    m_context->AddRef();
    return true;
}

void DX11Renderer::Shutdown() {
    if (m_context) { m_context->Release(); m_context = nullptr; }
    if (m_device) { m_device->Release(); m_device = nullptr; }
}

TextureHandle DX11Renderer::CreateTexture(uint32_t width, uint32_t height, uint32_t format, bool uav) {
    TextureHandle handle = {};
    if (!m_device) return handle;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = static_cast<DXGI_FORMAT>(format);
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (uav) {
        desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
    }

    ID3D11Texture2D* tex = nullptr;
    if (SUCCEEDED(m_device->CreateTexture2D(&desc, nullptr, &tex))) {
        handle.nativePtr = tex;
        handle.width = width;
        handle.height = height;

        if (uav) {
            ID3D11UnorderedAccessView* pUAV = nullptr;
            D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.Format = desc.Format;
            uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
            if (SUCCEEDED(m_device->CreateUnorderedAccessView(tex, &uavDesc, &pUAV))) {
                handle.nativeView = pUAV;
            }
        }
    }
    return handle;
}

void DX11Renderer::DestroyTexture(TextureHandle& handle) {
    if (handle.nativeView) {
        static_cast<ID3D11UnorderedAccessView*>(handle.nativeView)->Release();
        handle.nativeView = nullptr;
    }
    if (handle.nativePtr) {
        static_cast<ID3D11Texture2D*>(handle.nativePtr)->Release();
        handle.nativePtr = nullptr;
    }
}

ShaderHandle DX11Renderer::LoadComputeShader(const uint8_t* bytecode, size_t bytecodeSize) {
    ShaderHandle handle = {};
    if (!m_device || !bytecode || bytecodeSize == 0) return handle;

    // DX11 bytecode integrity check
    ID3D11ShaderReflection* reflector = nullptr;
    if (FAILED(D3DReflect(bytecode, bytecodeSize, IID_PPV_ARGS(&reflector)))) {
        LOG_ERROR("Shader bytecode integrity check failed (D3DReflect)");
        return handle;
    }
    reflector->Release();

    // S5.2: Cryptographic Shader Verification
    std::wstring expectedHash = EXPECTED_SHADER_HASH;
    if (!expectedHash.empty()) {
        std::wstring actualHash = ComputeShaderHashSHA256(bytecode, bytecodeSize);
        if (actualHash.empty() || _wcsicmp(actualHash.c_str(), expectedHash.c_str()) != 0) {
            LOG_ERROR("Shader bytecode integrity check failed (SHA-256 mismatch). Expected: %S", expectedHash.c_str());
            return handle;
        }
    }

    ID3D11ComputeShader* cs = nullptr;
    HRESULT hr = m_device->CreateComputeShader(bytecode, bytecodeSize, nullptr, &cs);
    if (SUCCEEDED(hr)) {
        handle.pipelineState = cs;
        handle.shaderBytecode = (void*)bytecode;
    } else {
        LOG_ERROR("DX11 CreateComputeShader failed with HRESULT 0x%X", hr);
    }
    return handle;
}

void DX11Renderer::DestroyShader(ShaderHandle& handle) {
    if (handle.pipelineState) {
        static_cast<ID3D11ComputeShader*>(handle.pipelineState)->Release();
        handle.pipelineState = nullptr;
    }
}

void DX11Renderer::DispatchCompute(ShaderHandle shader, TextureHandle input, TextureHandle output, uint32_t groupsX, uint32_t groupsY) {
    if (!m_context || !shader.pipelineState) return;

    ID3D11ComputeShader* cs = static_cast<ID3D11ComputeShader*>(shader.pipelineState);
    m_context->CSSetShader(cs, nullptr, 0);

    if (output.nativeView) {
        ID3D11UnorderedAccessView* uav = static_cast<ID3D11UnorderedAccessView*>(output.nativeView);
        m_context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
    }

    m_context->Dispatch(groupsX, groupsY, 1);

    if (output.nativeView) {
        ID3D11UnorderedAccessView* nullUAV = nullptr;
        m_context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
    }
}

void DX11Renderer::CopyToSwapchain(TextureHandle source, void* swapchainTexture) {
    if (!m_context || !source.nativePtr || !swapchainTexture) return;
    m_context->CopyResource(static_cast<ID3D11Resource*>(swapchainTexture), static_cast<ID3D11Resource*>(source.nativePtr));
}
