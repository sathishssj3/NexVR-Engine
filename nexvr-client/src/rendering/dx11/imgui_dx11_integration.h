#pragma once
#include <d3d11.h>
#include <wrl/client.h>

namespace vrinject {

class ImGuiDX11Integration {
public:
    static ImGuiDX11Integration& GetInstance() {
        static ImGuiDX11Integration instance;
        return instance;
    }

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context);
    void Render(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11RenderTargetView* rtv);
    void Render(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D* leftDest, ID3D11Texture2D* rightDest);
    void RenderToTexture(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D* targetTexture);
    void Shutdown();
    void InvalidateDeviceObjects();
    void CreateDeviceObjects();

    bool IsInitialized() const { return m_initialized; }

private:
    ImGuiDX11Integration() = default;
    ~ImGuiDX11Integration() = default;

    bool m_initialized = false;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_cachedRtvLeft;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_cachedTextureLeft;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_cachedRtvRight;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_cachedTextureRight;
};

} // namespace vrinject
