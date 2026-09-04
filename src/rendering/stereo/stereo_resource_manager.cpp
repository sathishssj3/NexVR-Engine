#include "rendering/stereo/stereo_resource_manager.h"
#include "core/logger.h"

// Include the compiled shader byte code
// This assumes CMake generated bin/shaders/stereo_reprojection_cs_dx11.h
#include "stereo_reprojection_cs_dx11.h"
#include "asw_shader_cs_dx11.h"

namespace vrinject {

StereoResourceManager::StereoResourceManager(ID3D11Device* device)
    : device_(device) {
}

StereoResourceManager::~StereoResourceManager() {
    ReleaseResources();
}

namespace {

// A depth-stencil texture cannot be read through an SRV in its typed form.
// Map the typed depth format to the typeless format a shadow copy must be
// created with, and to the format the SRV reads it back through.
DXGI_FORMAT TypelessDepthFormat(DXGI_FORMAT format) {
    switch (format) {
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        case DXGI_FORMAT_R32G8X24_TYPELESS:      return DXGI_FORMAT_R32G8X24_TYPELESS;
        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_R32_TYPELESS:           return DXGI_FORMAT_R32_TYPELESS;
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        case DXGI_FORMAT_R24G8_TYPELESS:         return DXGI_FORMAT_R24G8_TYPELESS;
        case DXGI_FORMAT_D16_UNORM:
        case DXGI_FORMAT_R16_TYPELESS:           return DXGI_FORMAT_R16_TYPELESS;
        default:                                 return format;   // already colour-like
    }
}

DXGI_FORMAT DepthSrvFormat(DXGI_FORMAT format) {
    switch (format) {
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        case DXGI_FORMAT_R32G8X24_TYPELESS:      return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_R32_TYPELESS:           return DXGI_FORMAT_R32_FLOAT;
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        case DXGI_FORMAT_R24G8_TYPELESS:         return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        case DXGI_FORMAT_D16_UNORM:
        case DXGI_FORMAT_R16_TYPELESS:           return DXGI_FORMAT_R16_UNORM;
        default:                                 return format;
    }
}

} // namespace

void StereoResourceManager::ReleaseResources() {
    leftEyeTex_.Reset();
    rightEyeTex_.Reset();
    leftEyeUAV_.Reset();
    rightEyeUAV_.Reset();
    reprojectionShader_.Reset();
    aswShader_.Reset();
    constantBuffer_.Reset();
    gameColorCopy_.Reset();
    gameDepthCopy_.Reset();
    gameColorSRV_.Reset();
    gameDepthSRV_.Reset();
    width_ = 0;
    height_ = 0;
    format_ = DXGI_FORMAT_UNKNOWN;
}

bool StereoResourceManager::EnsureOneSourceView(
        ID3D11DeviceContext* context,
        ID3D11Texture2D* source,
        DXGI_FORMAT shadowFormat,
        DXGI_FORMAT srvFormat,
        Microsoft::WRL::ComPtr<ID3D11Texture2D>& shadow,
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv) {
    if (!device_ || !context || !source) return false;

    D3D11_TEXTURE2D_DESC srcDesc = {};
    source->GetDesc(&srcDesc);

    // MSAA sources need ResolveSubresource, not CopyResource. Rather than
    // silently produce a wrong image, decline and let the caller fall back.
    if (srcDesc.SampleDesc.Count > 1) {
        LOG_WARN("StereoResourceManager: source is MSAA x%u, unsupported", srcDesc.SampleDesc.Count);
        return false;
    }

    // Always use shadow copy to avoid creating SRVs directly on the game's swapchain backbuffer,
    // which would hold persistent COM references and cause DXGI_ERROR_INVALID_CALL during ResizeBuffers.

    // Slow path: rebuild the shadow copy if absent or mismatched.
    bool rebuild = !shadow;
    if (shadow) {
        D3D11_TEXTURE2D_DESC have = {};
        shadow->GetDesc(&have);
        rebuild = have.Width != srcDesc.Width || have.Height != srcDesc.Height;
    }

    if (rebuild) {
        shadow.Reset();
        srv.Reset();

        D3D11_TEXTURE2D_DESC desc = srcDesc;
        desc.Format = (shadowFormat == DXGI_FORMAT_UNKNOWN) ? srcDesc.Format : shadowFormat;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.MipLevels = 1;
        desc.ArraySize = 1;

        if (FAILED(device_->CreateTexture2D(&desc, nullptr, &shadow))) {
            LOG_ERROR("StereoResourceManager: shadow texture creation failed (format %u)", desc.Format);
            shadow.Reset();
            return false;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
        viewDesc.Format = (srvFormat == DXGI_FORMAT_UNKNOWN) ? desc.Format : srvFormat;
        viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        viewDesc.Texture2D.MipLevels = 1;

        if (FAILED(device_->CreateShaderResourceView(shadow.Get(), &viewDesc, &srv))) {
            LOG_ERROR("StereoResourceManager: shadow SRV creation failed (format %u)", viewDesc.Format);
            shadow.Reset();
            srv.Reset();
            return false;
        }
    }

    context->CopyResource(shadow.Get(), source);
    return srv != nullptr;
}

bool StereoResourceManager::EnsureSourceViews(ID3D11DeviceContext* context,
                                              ID3D11Texture2D* gameColor,
                                              ID3D11Texture2D* gameDepth) {
    if (!gameColor) return false;

    const bool colorOk = EnsureOneSourceView(
        context, gameColor,
        DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN,
        gameColorCopy_, gameColorSRV_);

    bool depthOk = false;
    if (gameDepth) {
        // Colour inherits its own format; depth needs the typeless/read pair.
        D3D11_TEXTURE2D_DESC depthDesc = {};
        gameDepth->GetDesc(&depthDesc);

        depthOk = EnsureOneSourceView(
            context, gameDepth,
            TypelessDepthFormat(depthDesc.Format), DepthSrvFormat(depthDesc.Format),
            gameDepthCopy_, gameDepthSRV_);
    } else {
        // Create 1x1 dummy depth texture if depth is not available yet (e.g. main menu)
        if (!gameDepthCopy_ || !gameDepthSRV_) {
            D3D11_TEXTURE2D_DESC dummyDesc = {};
            dummyDesc.Width = 1;
            dummyDesc.Height = 1;
            dummyDesc.MipLevels = 1;
            dummyDesc.ArraySize = 1;
            dummyDesc.Format = DXGI_FORMAT_R32_FLOAT;
            dummyDesc.SampleDesc.Count = 1;
            dummyDesc.Usage = D3D11_USAGE_DEFAULT;
            dummyDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            
            float farVal = 1.0f;
            D3D11_SUBRESOURCE_DATA initData = { &farVal, sizeof(float), 0 };
            if (SUCCEEDED(device_->CreateTexture2D(&dummyDesc, &initData, &gameDepthCopy_))) {
                device_->CreateShaderResourceView(gameDepthCopy_.Get(), nullptr, &gameDepthSRV_);
            }
        }
        depthOk = (gameDepthSRV_ != nullptr);
    }

    return colorOk && depthOk;
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
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    DXGI_FORMAT texFormat = format;
    DXGI_FORMAT uavFormat = format;
    if (format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) {
        texFormat = DXGI_FORMAT_R8G8B8A8_TYPELESS;
        uavFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    } else if (format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) {
        texFormat = DXGI_FORMAT_B8G8R8A8_TYPELESS;
        uavFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
    } else if (format == DXGI_FORMAT_UNKNOWN) {
        texFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        uavFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    }
    desc.Format = texFormat;

    HRESULT hr = device_->CreateTexture2D(&desc, nullptr, &leftEyeTex_);
    if (FAILED(hr)) {
        LOG_ERROR("StereoResourceManager: CreateTexture2D for leftEyeTex failed (format %u, hr=0x%X)", texFormat, hr);
        return false;
    }

    hr = device_->CreateTexture2D(&desc, nullptr, &rightEyeTex_);
    if (FAILED(hr)) {
        LOG_ERROR("StereoResourceManager: CreateTexture2D for rightEyeTex failed (format %u, hr=0x%X)", texFormat, hr);
        return false;
    }

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = uavFormat;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Texture2D.MipSlice = 0;

    hr = device_->CreateUnorderedAccessView(leftEyeTex_.Get(), &uavDesc, &leftEyeUAV_);
    if (FAILED(hr)) {
        LOG_ERROR("StereoResourceManager: CreateUnorderedAccessView for leftEyeUAV failed (format %u, hr=0x%X)", uavFormat, hr);
        return false;
    }

    hr = device_->CreateUnorderedAccessView(rightEyeTex_.Get(), &uavDesc, &rightEyeUAV_);
    if (FAILED(hr)) {
        LOG_ERROR("StereoResourceManager: CreateUnorderedAccessView for rightEyeUAV failed (format %u, hr=0x%X)", uavFormat, hr);
        return false;
    }

    return true;
}

bool StereoResourceManager::LoadShaders() {
    HRESULT hr = device_->CreateComputeShader(g_stereo_reprojection_DX11, sizeof(g_stereo_reprojection_DX11), nullptr, &reprojectionShader_);
    if (FAILED(hr)) return false;
    hr = device_->CreateComputeShader(g_asw_shader_DX11, sizeof(g_asw_shader_DX11), nullptr, &aswShader_);
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
