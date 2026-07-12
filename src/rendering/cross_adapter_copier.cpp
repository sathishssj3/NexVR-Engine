#include "cross_adapter_copier.h"
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include "../core/logger.h"

namespace vrinject {

bool CrossAdapterCopier::Initialize(ID3D11Device* gameDevice, ID3D11Device* proxyDevice, UINT width, UINT height, DXGI_FORMAT format) {
    if (m_initialized) return true;

    // Create a shared texture on the game's GPU
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    
    // NTHANDLE allows sharing across devices/adapters
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

    HRESULT hr = gameDevice->CreateTexture2D(&desc, nullptr, &m_gameSharedTex);
    if (FAILED(hr)) {
        LOG_ERROR("CrossAdapterCopier: Failed to create game shared texture. HR: 0x%08X", hr);
        return false;
    }

    // Get the shared NT handle
    Microsoft::WRL::ComPtr<IDXGIResource1> dxgiResource;
    hr = m_gameSharedTex.As(&dxgiResource);
    if (FAILED(hr)) {
        LOG_ERROR("CrossAdapterCopier: Failed to get IDXGIResource1.");
        return false;
    }

    hr = dxgiResource->CreateSharedHandle(
        nullptr,
        DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
        nullptr,
        &m_sharedHandle
    );

    if (FAILED(hr)) {
        LOG_ERROR("CrossAdapterCopier: Failed to create shared handle. HR: 0x%08X", hr);
        return false;
    }

    // Open the shared texture on the proxy device
    Microsoft::WRL::ComPtr<ID3D11Device1> proxyDevice1;
    hr = proxyDevice->QueryInterface(__uuidof(ID3D11Device1), &proxyDevice1);
    if (FAILED(hr)) {
        LOG_ERROR("CrossAdapterCopier: Proxy device does not support ID3D11Device1.");
        return false;
    }

    hr = proxyDevice1->OpenSharedResource1(m_sharedHandle, __uuidof(ID3D11Texture2D), (void**)&m_proxySharedTex);
    if (FAILED(hr)) {
        LOG_ERROR("CrossAdapterCopier: Failed to open shared texture on proxy device. HR: 0x%08X", hr);
        return false;
    }

    // Get the keyed mutexes for synchronization
    hr = m_gameSharedTex.As(&m_gameMutex);
    if (FAILED(hr)) {
        LOG_ERROR("CrossAdapterCopier: Failed to get game mutex.");
        return false;
    }

    hr = m_proxySharedTex.As(&m_proxyMutex);
    if (FAILED(hr)) {
        LOG_ERROR("CrossAdapterCopier: Failed to get proxy mutex.");
        return false;
    }

    m_initialized = true;
    LOG_INFO("CrossAdapterCopier: Initialized successfully for %ux%u format %u", width, height, format);
    return true;
}

void CrossAdapterCopier::Shutdown() {
    if (m_sharedHandle) {
        CloseHandle(m_sharedHandle);
        m_sharedHandle = NULL;
    }
    m_gameSharedTex.Reset();
    m_proxySharedTex.Reset();
    m_gameMutex.Reset();
    m_proxyMutex.Reset();
    m_initialized = false;
}

bool CrossAdapterCopier::CopyFrame(ID3D11DeviceContext* gameContext, ID3D11DeviceContext* proxyContext, ID3D11Resource* gameSourceTex, ID3D11Resource* proxyDestTex) {
    if (!m_initialized || !gameSourceTex || !proxyDestTex) return false;

    // 1. Game GPU: Acquire mutex, copy from game swapchain -> game shared texture, release mutex
    HRESULT hr = m_gameMutex->AcquireSync(0, 16); // Wait up to 16ms
    if (hr != S_OK) return false;

    gameContext->CopyResource(m_gameSharedTex.Get(), gameSourceTex);
    
    // Ensure the copy is dispatched before we release
    gameContext->Flush();
    m_gameMutex->ReleaseSync(1); // Release to state 1

    // 2. Proxy GPU: Acquire mutex (waiting for state 1), copy from proxy shared texture -> OpenXR swapchain, release mutex
    hr = m_proxyMutex->AcquireSync(1, 16); // Wait for state 1
    if (hr != S_OK) {
        // If we timed out, we still need to reset the mutex state on the game side for the next frame
        if (m_gameMutex->AcquireSync(1, 0) == S_OK) {
            m_gameMutex->ReleaseSync(0);
        }
        return false;
    }

    proxyContext->CopyResource(proxyDestTex, m_proxySharedTex.Get());
    proxyContext->Flush();
    
    m_proxyMutex->ReleaseSync(0); // Release back to state 0

    return true;
}

} // namespace vrinject
