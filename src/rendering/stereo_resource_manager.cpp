#include "stereo_resource_manager.h"
#include "../core/logger.h"

// Include the compiled shader byte code
// This assumes CMake generated bin/shaders/stereo_reprojection_cs_dx11.h
#include "stereo_reprojection_cs_dx11.h"

namespace vrinject {

StereoResourceManager::StereoResourceManager(ID3D11Device* device)
    : device_(device) {
}

StereoResourceManager::~StereoResourceManager() {
    ReleaseResources();
}

void StereoResourceManager::ReleaseResources() {
    leftEyeTex_.Reset();
    rightEyeTex_.Reset();
    leftEyeUAV_.Reset();
    rightEyeUAV_.Reset();
    reprojectionShader_.Reset();
    constantBuffer_.Reset();
    width_ = 0;
    height_ = 0;
    format_ = DXGI_FORMAT_UNKNOWN;
}

bool StereoResourceManager::Initialize(uint32_t width, uint32_t height, DXGI_FORMAT format) {
    if (!device_) return false;

    // Check if we already have resources with matching dimensions/format
    if (width_ == width && height_ == height && format_ == format && leftEyeTex_) {
        return true;
    }

    ReleaseResources();

    if (!CreateRenderTargets(width, height, format)) {
        LOG_ERROR("StereoResourceManager: Failed to create render targets");
        ReleaseResources();
        return false;
    }

    if (!LoadShaders()) {
        LOG_ERROR("StereoResourceManager: Failed to load stereo reprojection shader");
        ReleaseResources();
        return false;
    }

    if (!CreateConstantBuffer()) {
        LOG_ERROR("StereoResourceManager: Failed to create constant buffer");
        ReleaseResources();
        return false;
    }

    width_ = width;
    height_ = height;
    format_ = format;

    LOG_INFO("StereoResourceManager: Successfully initialized %ux%u format %u", width, height, format);
    return true;
}

bool StereoResourceManager::CreateRenderTargets(uint32_t width, uint32_t height, DXGI_FORMAT format) {
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    HRESULT hr = device_->CreateTexture2D(&desc, nullptr, &leftEyeTex_);
    if (FAILED(hr)) return false;

    hr = device_->CreateTexture2D(&desc, nullptr, &rightEyeTex_);
    if (FAILED(hr)) return false;

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = format;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Texture2D.MipSlice = 0;

    hr = device_->CreateUnorderedAccessView(leftEyeTex_.Get(), &uavDesc, &leftEyeUAV_);
    if (FAILED(hr)) return false;

    hr = device_->CreateUnorderedAccessView(rightEyeTex_.Get(), &uavDesc, &rightEyeUAV_);
    if (FAILED(hr)) return false;

    return true;
}

bool StereoResourceManager::LoadShaders() {
    HRESULT hr = device_->CreateComputeShader(g_stereo_reprojection_DX11, sizeof(g_stereo_reprojection_DX11), nullptr, &reprojectionShader_);
    return SUCCEEDED(hr);
}

bool StereoResourceManager::CreateConstantBuffer() {
    // Needs to be 16-byte aligned. 
    // In HLSL, the constant buffer will match StereoFrameContext fields we care about.
    // Let's make it 256 bytes for safety and flexibility.
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.ByteWidth = 256; 
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbDesc.MiscFlags = 0;
    cbDesc.StructureByteStride = 0;

    HRESULT hr = device_->CreateBuffer(&cbDesc, nullptr, &constantBuffer_);
    return SUCCEEDED(hr);
}

} // namespace vrinject
