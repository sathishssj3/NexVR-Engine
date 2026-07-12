#pragma once

#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

namespace vrinject {

class CrossAdapterCopier {
public:
    CrossAdapterCopier() = default;
    ~CrossAdapterCopier() { Shutdown(); }

    bool Initialize(ID3D11Device* gameDevice, ID3D11Device* proxyDevice, UINT width, UINT height, DXGI_FORMAT format);
    void Shutdown();

    // Copies a texture from the game device to the proxy device
    bool CopyFrame(ID3D11DeviceContext* gameContext, ID3D11DeviceContext* proxyContext, ID3D11Resource* gameSourceTex, ID3D11Resource* proxyDestTex);

    bool IsInitialized() const { return m_initialized; }
    ID3D11Texture2D* GetGameSharedTexture() const { return m_gameSharedTex.Get(); }

private:
    bool m_initialized = false;
    
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_gameSharedTex;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_proxySharedTex;
    
    Microsoft::WRL::ComPtr<IDXGIKeyedMutex> m_gameMutex;
    Microsoft::WRL::ComPtr<IDXGIKeyedMutex> m_proxyMutex;

    HANDLE m_sharedHandle = NULL;
};

} // namespace vrinject
