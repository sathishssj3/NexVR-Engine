#include "dxgi_factory_hook.h"
#include "MinHook.h"
#include "../core/logger.h"
#include <mutex>

namespace vrinject {
namespace DXGIFactoryHook {

typedef HRESULT(__stdcall* CreateSwapChain_t)(IDXGIFactory*, IUnknown*, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**);
typedef HRESULT(__stdcall* CreateSwapChainForHwnd_t)(IDXGIFactory2*, IUnknown*, HWND, const DXGI_SWAP_CHAIN_DESC1*, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*, IDXGISwapChain1**);

CreateSwapChain_t OriginalCreateSwapChain = nullptr;
CreateSwapChainForHwnd_t OriginalCreateSwapChainForHwnd = nullptr;

Microsoft::WRL::ComPtr<ID3D12CommandQueue> g_capturedCommandQueue;
std::mutex g_mutex;

HRESULT __stdcall hkCreateSwapChain(IDXGIFactory* pFactory, IUnknown* pDevice, DXGI_SWAP_CHAIN_DESC* pDesc, IDXGISwapChain** ppSwapChain) {
    LOG_INFO("DXGIFactoryHook: CreateSwapChain called");

    DXGI_SWAP_CHAIN_DESC modifiedDesc = {};
    if (pDesc) {
        modifiedDesc = *pDesc;
        LOG_INFO("DXGIFactoryHook: CreateSwapChain requested format %d", modifiedDesc.BufferDesc.Format);
        // Only force R8G8B8A8_UNORM if the requested format is not already a supported format.
        // This preserves HDR/10-bit formats requested by the game.
        modifiedDesc.BufferUsage |= DXGI_USAGE_SHADER_INPUT;
    }

    HRESULT hr = OriginalCreateSwapChain(pFactory, pDevice, pDesc ? &modifiedDesc : nullptr, ppSwapChain);
    if (SUCCEEDED(hr) && pDevice) {
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue;
        if (SUCCEEDED(pDevice->QueryInterface(__uuidof(ID3D12CommandQueue), (void**)&queue))) {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_capturedCommandQueue = queue;
            LOG_INFO("DXGIFactoryHook: Captured ID3D12CommandQueue via CreateSwapChain");
        }

    }
    LOG_INFO("DXGIFactoryHook: CreateSwapChain returned hr=0x%X", hr);
    return hr;
}

HRESULT __stdcall hkCreateSwapChainForHwnd(IDXGIFactory2* pFactory, IUnknown* pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1* pDesc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc, IDXGIOutput* pRestrictToOutput, IDXGISwapChain1** ppSwapChain) {
    LOG_INFO("DXGIFactoryHook: CreateSwapChainForHwnd called");

    DXGI_SWAP_CHAIN_DESC1 modifiedDesc = {};
    if (pDesc) {
        modifiedDesc = *pDesc;
        LOG_INFO("DXGIFactoryHook: CreateSwapChainForHwnd requested format %d", modifiedDesc.Format);
        // Only force R8G8B8A8_UNORM if the requested format is not already a supported format.
        // This preserves HDR/10-bit formats requested by the game.
        modifiedDesc.BufferUsage |= DXGI_USAGE_SHADER_INPUT;
    }

    HRESULT hr = OriginalCreateSwapChainForHwnd(pFactory, pDevice, hWnd, pDesc ? &modifiedDesc : nullptr, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
    if (SUCCEEDED(hr) && pDevice) {
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue;
        if (SUCCEEDED(pDevice->QueryInterface(__uuidof(ID3D12CommandQueue), (void**)&queue))) {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_capturedCommandQueue = queue;
            LOG_INFO("DXGIFactoryHook: Captured ID3D12CommandQueue via CreateSwapChainForHwnd");
        }

    }
    LOG_INFO("DXGIFactoryHook: CreateSwapChainForHwnd returned hr=0x%X", hr);
    return hr;
}

bool Initialize() {
    IDXGIFactory2* pFactory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory2), (void**)&pFactory))) {
        LOG_ERROR("DXGIFactoryHook: Failed to create IDXGIFactory2");
        return false;
    }

    void** vtable = *reinterpret_cast<void***>(pFactory);
    void* createSwapChainAddress = vtable[10]; // IDXGIFactory::CreateSwapChain
    void* createSwapChainForHwndAddress = vtable[15]; // IDXGIFactory2::CreateSwapChainForHwnd

    pFactory->Release();

    if (MH_CreateHook(createSwapChainAddress, (void*)hkCreateSwapChain, (void**)&OriginalCreateSwapChain) != MH_OK) {
        LOG_ERROR("DXGIFactoryHook: Failed to hook CreateSwapChain");
        return false;
    }

    if (MH_CreateHook(createSwapChainForHwndAddress, (void*)hkCreateSwapChainForHwnd, (void**)&OriginalCreateSwapChainForHwnd) != MH_OK) {
        LOG_ERROR("DXGIFactoryHook: Failed to hook CreateSwapChainForHwnd");
        return false;
    }

    if (MH_EnableHook(createSwapChainAddress) != MH_OK || MH_EnableHook(createSwapChainForHwndAddress) != MH_OK) {
        LOG_ERROR("DXGIFactoryHook: Failed to enable hooks");
        return false;
    }

    LOG_INFO("DXGIFactoryHook: Initialized successfully");
    return true;
}

void Shutdown() {
    MH_DisableHook(MH_ALL_HOOKS);
    std::lock_guard<std::mutex> lock(g_mutex);
    g_capturedCommandQueue.Reset();
}

ID3D12CommandQueue* GetCapturedCommandQueue() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_capturedCommandQueue.Get();
}

}
}
