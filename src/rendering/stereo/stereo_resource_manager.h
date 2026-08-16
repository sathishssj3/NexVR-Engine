#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>

namespace vrinject {

class StereoResourceManager {
public:
    StereoResourceManager(ID3D11Device* device);
    ~StereoResourceManager();

    bool Initialize(uint32_t width, uint32_t height, DXGI_FORMAT format);
    void ReleaseResources();

    ID3D11Texture2D* GetLeftEyeTexture() const { return leftEyeTex_.Get(); }
    ID3D11Texture2D* GetRightEyeTexture() const { return rightEyeTex_.Get(); }
    ID3D11UnorderedAccessView* GetLeftEyeUAV() const { return leftEyeUAV_.Get(); }
    ID3D11UnorderedAccessView* GetRightEyeUAV() const { return rightEyeUAV_.Get(); }
    
    ID3D11ComputeShader* GetReprojectionShader() const { return reprojectionShader_.Get(); }
    ID3D11ComputeShader* GetAswShader() const { return aswShader_.Get(); }
    ID3D11Buffer* GetConstantBuffer() const { return constantBuffer_.Get(); }

    // Produces shader-readable views of the game's colour and depth textures,
    // which the reprojection compute shader samples as t0/t1.
    //
    // Neither can generally be bound directly: a swapchain backbuffer is created
    // RENDER_TARGET without BIND_SHADER_RESOURCE, and a depth-stencil is a
    // non-SRV-able typed format. So when a source lacks BIND_SHADER_RESOURCE we
    // keep a shadow copy carrying the right bind flags and CopyResource into it
    // each frame; when the game already made it shader-readable we bind it
    // directly and skip the copy.
    //
    // Returns false if either view could not be produced - callers must treat
    // that as "no stereo this frame" rather than dispatching with nulls.
    bool EnsureSourceViews(ID3D11DeviceContext* context,
                           ID3D11Texture2D* gameColor,
                           ID3D11Texture2D* gameDepth);

    ID3D11ShaderResourceView* GetGameColorSRV() const { return gameColorSRV_.Get(); }
    ID3D11ShaderResourceView* GetGameDepthSRV() const { return gameDepthSRV_.Get(); }

    uint32_t GetWidth() const { return width_; }
    uint32_t GetHeight() const { return height_; }
    DXGI_FORMAT GetFormat() const { return format_; }

private:
    bool CreateRenderTargets(uint32_t width, uint32_t height, DXGI_FORMAT format);
    bool LoadShaders();
    bool CreateConstantBuffer();

    // Binds `source` directly when it is already shader-readable, otherwise
    // lazily (re)creates `shadow` to match and copies into it. `srvFormat` is
    // DXGI_FORMAT_UNKNOWN for colour (inherit) and the depth-read format for
    // depth (e.g. R32_FLOAT for a D32_FLOAT source).
    bool EnsureOneSourceView(ID3D11DeviceContext* context,
                             ID3D11Texture2D* source,
                             DXGI_FORMAT shadowFormat,
                             DXGI_FORMAT srvFormat,
                             Microsoft::WRL::ComPtr<ID3D11Texture2D>& shadow,
                             Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv);

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    DXGI_FORMAT format_ = DXGI_FORMAT_UNKNOWN;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> leftEyeTex_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> rightEyeTex_;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> leftEyeUAV_;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> rightEyeUAV_;
    
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> reprojectionShader_;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> aswShader_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer_;

    // Shadow copies of the game's colour/depth, used only when the originals
    // are not shader-readable. Kept across frames and rebuilt on size change.
    Microsoft::WRL::ComPtr<ID3D11Texture2D> gameColorCopy_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> gameDepthCopy_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> gameColorSRV_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> gameDepthSRV_;
};

} // namespace vrinject
