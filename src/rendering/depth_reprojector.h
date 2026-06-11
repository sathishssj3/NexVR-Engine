#pragma once
#include <d3d11.h>
#include <wrl/client.h>

namespace vrinject {

struct StereoParams;

struct ReprojectionConstants {
    float IPD;
    float Convergence;
    float NearPlane;
    float FarPlane;
    float Width;
    float Height;
    float ReversedZ;
    float Padding;
};

class DepthReprojector {
public:
    bool Initialize(ID3D11Device* device);
    void Reproject(ID3D11DeviceContext* context, 
                   ID3D11ShaderResourceView* leftEyeColor,
                   ID3D11ShaderResourceView* leftEyeDepth,
                   ID3D11UnorderedAccessView* rightEyeOutputUAV,
                   const StereoParams& params);

private:
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_computeShader;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_constantBuffer;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_samplerState;
};

} // namespace vrinject
