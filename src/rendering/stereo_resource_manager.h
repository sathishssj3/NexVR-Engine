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
    ID3D11Buffer* GetConstantBuffer() const { return constantBuffer_.Get(); }

    uint32_t GetWidth() const { return width_; }
    uint32_t GetHeight() const { return height_; }
    DXGI_FORMAT GetFormat() const { return format_; }

private:
    bool CreateRenderTargets(uint32_t width, uint32_t height, DXGI_FORMAT format);
    bool LoadShaders();
    bool CreateConstantBuffer();

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    DXGI_FORMAT format_ = DXGI_FORMAT_UNKNOWN;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> leftEyeTex_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> rightEyeTex_;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> leftEyeUAV_;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> rightEyeUAV_;
    
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> reprojectionShader_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer_;
};

} // namespace vrinject
