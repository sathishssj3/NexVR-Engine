#include "dx11_hook.h"
#include <dxgi1_2.h>
#include "MinHook.h"
#include <mutex>
#include <atomic>
#include <memory>
#include <string>
#include <d3d11.h>
#include <dxgi1_2.h>

#include "../core/logger.h"
#include "../core/seh_shield.h"
#include "../core/config_manager.h"
#include "../core/dx11_lifecycle_manager.h"
#include "../core/dx12_lifecycle_manager.h"
#include "../core/frame_coordinator.h"
#include "../core/candidate_collector.h"
#include "../core/depth_candidate_collector.h"
#include "../core/depth_lock_manager.h"
#include "dxgi_factory_hook.h"
extern HMODULE g_hModule; // Declared in dllmain.cpp

namespace vrinject {
namespace DX11Hook {

typedef HRESULT(__stdcall* Present_t)(IDXGISwapChain*, UINT, UINT);
typedef HRESULT(__stdcall* Present1_t)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
typedef HRESULT(__stdcall* ResizeBuffers_t)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
typedef void(__stdcall* UpdateSubresource_t)(ID3D11DeviceContext*, ID3D11Resource*, UINT, const D3D11_BOX*, const void*, UINT, UINT);
typedef HRESULT(__stdcall* Map_t)(ID3D11DeviceContext*, ID3D11Resource*, UINT, D3D11_MAP, UINT, D3D11_MAPPED_SUBRESOURCE*);
typedef void(__stdcall* OMSetRenderTargets_t)(ID3D11DeviceContext*, UINT, ID3D11RenderTargetView *const *, ID3D11DepthStencilView *);
typedef void(__stdcall* ClearDepthStencilView_t)(ID3D11DeviceContext*, ID3D11DepthStencilView *, UINT, FLOAT, UINT8);
typedef HRESULT(__stdcall* CreateTexture2D_t)(ID3D11Device*, const D3D11_TEXTURE2D_DESC *, const D3D11_SUBRESOURCE_DATA *, ID3D11Texture2D **);

Present_t OriginalPresent = nullptr;
Present1_t OriginalPresent1 = nullptr;
ResizeBuffers_t OriginalResizeBuffers = nullptr;
UpdateSubresource_t OriginalUpdateSubresource = nullptr;
Map_t OriginalMap = nullptr;
OMSetRenderTargets_t OriginalOMSetRenderTargets = nullptr;
ClearDepthStencilView_t OriginalClearDepthStencilView = nullptr;
CreateTexture2D_t OriginalCreateTexture2D = nullptr;

static void* g_targetPresent = nullptr;
static void* g_targetPresent1 = nullptr;
static void* g_targetResizeBuffers = nullptr;
static void* g_targetUpdateSubresource = nullptr;
static void* g_targetMap = nullptr;
static void* g_targetOMSetRenderTargets = nullptr;
static void* g_targetClearDepthStencilView = nullptr;
static void* g_targetCreateTexture2D = nullptr;


void VerifyHookIntegrity() {
    auto verify = [](void* target, const char* name) {
        if (!target) return;
        
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(target, &mbi, sizeof(mbi)) == 0 || 
            !(mbi.Protect & (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_READONLY | PAGE_READWRITE))) {
            return; // Not readable memory
        }

        uint8_t* code = reinterpret_cast<uint8_t*>(target);
        // MinHook detours start with relative JMP (0xE9), short JMP (0xEB), 
        // absolute JMP (0xFF 0x25 or 0xFF 0x15), or 64-bit absolute JMP (0x48 0xB8 ... 0xFF 0xE0).
        bool intact = (code[0] == 0xE9 || code[0] == 0xEB || 
                       (code[0] == 0xFF && (code[1] == 0x25 || code[1] == 0x15)) ||
                       (code[0] == 0x48 && code[1] == 0xB8 && code[10] == 0xFF && code[11] == 0xE0));
        if (!intact) {
            LOG_WARN("DX11Hook: Hook integrity check FAILED for %s! Attempting to re-enable hook.", name);
            MH_QueueEnableHook(target);
            MH_ApplyQueued();
        }
    };
    
    verify(g_targetPresent, "Present");
    verify(g_targetPresent1, "Present1");
    verify(g_targetResizeBuffers, "ResizeBuffers");
}

template<typename SwapChainType, typename OriginalFunc, typename... Args>
HRESULT ProcessPresent(SwapChainType* pSwapChain, OriginalFunc originalFunc, Args... args) {
    if (!vrinject::seh::IsValidMemoryPointer(pSwapChain) || !originalFunc) {
        return originalFunc ? originalFunc(pSwapChain, args...) : DXGI_ERROR_INVALID_CALL;
    }

    HRESULT hr = S_OK;
    try {
        // Execute original present first to get the true HRESULT (e.g. DEVICE_REMOVED)
        hr = originalFunc(pSwapChain, args...);

        // Check if this is a DX12 swapchain by looking for captured command queue
        ID3D12CommandQueue* capturedQueue = DXGIFactoryHook::GetCapturedCommandQueue();
        
        RenderFrameSnapshot snapshot;
        if (capturedQueue) {
            // DX12 path - use Dx12LifecycleManager
            snapshot = Dx12LifecycleManager::Get().ProcessPresent(
                static_cast<IDXGISwapChain*>(pSwapChain), capturedQueue, hr);
        } else {
            // DX11 fallback path
            snapshot = Dx11LifecycleManager::Get().ProcessPresent(pSwapChain, hr);
        }

        // Exception-safe RAII frame lifecycle
        ScopedFrame frame(FrameCoordinator::Get(), snapshot);

        // Periodically verify hook integrity to prevent external unhooking (e.g. by anti-cheats or overlays)
        static int s_frameCount = 0;
        if (++s_frameCount % 600 == 0) {
            VerifyHookIntegrity();
        }

    } catch (const std::exception& e) {
        LOG_ERROR("DX11Hook: ProcessPresent exception caught: %s", e.what());
    } catch (...) {
        LOG_ERROR("DX11Hook: ProcessPresent unknown exception caught");
    }

    return hr;
}


HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    return ProcessPresent(pSwapChain, OriginalPresent, SyncInterval, Flags);
}

HRESULT __stdcall hkPresent1(IDXGISwapChain1* pSwapChain, UINT SyncInterval, UINT PresentFlags, const DXGI_PRESENT_PARAMETERS* pPresentParameters) {
    return ProcessPresent(pSwapChain, OriginalPresent1, SyncInterval, PresentFlags, pPresentParameters);
}

HRESULT __stdcall hkResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) {
    if (OriginalResizeBuffers) {
        // Re-creating the swapchain / resizing. We don't have the resource pointer here, 
        // but we know that old depth buffers might be invalid. Handled by lifecycle manager.
        return OriginalResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
    }
    return DXGI_ERROR_INVALID_CALL;
}

static std::atomic<uint32_t> s_globalTextureGeneration{1};

HRESULT __stdcall hkCreateTexture2D(ID3D11Device* pDevice, const D3D11_TEXTURE2D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture2D** ppTexture2D) {
    HRESULT hr = OriginalCreateTexture2D ? OriginalCreateTexture2D(pDevice, pDesc, pInitialData, ppTexture2D) : DXGI_ERROR_INVALID_CALL;
    if (SUCCEEDED(hr) && ppTexture2D && *ppTexture2D && pDesc) {
        if (pDesc->BindFlags & D3D11_BIND_DEPTH_STENCIL) {
            uint32_t gen = s_globalTextureGeneration.fetch_add(1, std::memory_order_relaxed);
            DepthCandidateCollector::Get().OnDepthSurfaceCreated(
                *ppTexture2D, pDesc->Width, pDesc->Height, pDesc->Format, 
                pDesc->SampleDesc.Count, pDesc->ArraySize, pDesc->MipLevels, gen);
        }
    }
    return hr;
}

void __stdcall hkOMSetRenderTargets(ID3D11DeviceContext* pContext, UINT NumViews, ID3D11RenderTargetView *const *ppRenderTargetViews, ID3D11DepthStencilView *pDepthStencilView) {
    if (pDepthStencilView) {
        ID3D11Resource* pResource = nullptr;
        pDepthStencilView->GetResource(&pResource);
        if (pResource) {
            DepthCandidateCollector::Get().OnOMSetRenderTargets(pResource);
            pResource->Release(); // GetResource adds a ref
        }
    }
    if (OriginalOMSetRenderTargets) {
        OriginalOMSetRenderTargets(pContext, NumViews, ppRenderTargetViews, pDepthStencilView);
    }
}

void __stdcall hkClearDepthStencilView(ID3D11DeviceContext* pContext, ID3D11DepthStencilView *pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil) {
    if (pDepthStencilView && (ClearFlags & D3D11_CLEAR_DEPTH)) {
        ID3D11Resource* pResource = nullptr;
        pDepthStencilView->GetResource(&pResource);
        if (pResource) {
            DepthCandidateCollector::Get().OnClearDepthStencilView(pResource, Depth);
            pResource->Release();
        }
    }
    if (OriginalClearDepthStencilView) {
        OriginalClearDepthStencilView(pContext, pDepthStencilView, ClearFlags, Depth, Stencil);
    }
}

void __stdcall hkUpdateSubresource(ID3D11DeviceContext* pContext, ID3D11Resource* pDstResource, UINT DstSubresource, const D3D11_BOX* pDstBox, const void* pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch) {
    if (pDstResource && pSrcData) {
        D3D11_RESOURCE_DIMENSION dim;
        pDstResource->GetType(&dim);
        if (dim == D3D11_RESOURCE_DIMENSION_BUFFER) {
            ID3D11Buffer* pBuffer = static_cast<ID3D11Buffer*>(pDstResource);
            D3D11_BUFFER_DESC desc;
            pBuffer->GetDesc(&desc);
            if (desc.BindFlags & D3D11_BIND_CONSTANT_BUFFER) {
                CandidateCollector::Get().OnConstantBufferUpdate(pBuffer, pSrcData, desc.ByteWidth);
            }
        }
    }
    if (OriginalUpdateSubresource) {
        OriginalUpdateSubresource(pContext, pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);
    }
}

HRESULT __stdcall hkMap(ID3D11DeviceContext* pContext, ID3D11Resource* pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource) {
    HRESULT hr = OriginalMap ? OriginalMap(pContext, pResource, Subresource, MapType, MapFlags, pMappedResource) : DXGI_ERROR_INVALID_CALL;
    
    // We can't safely intercept map writes because they happen AFTER Map returns, via CPU pointer.
    // However, some games use Map(WRITE_DISCARD) and write immediately.
    // To truly capture Map, we'd need to intercept Unmap. For now, we skip Map or just rely on UpdateSubresource.
    // We'll leave it as a passthrough for now, as most camera CBs are UpdateSubresource or Map/Unmap.
    // Ideally we hook Unmap to read the data, but Unmap doesn't give us the data pointer. 
    // We would need to track Map returns. Let's just hook UpdateSubresource for now.
    
    return hr;
}

bool Initialize() {
    MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
        LOG_ERROR("DX11Hook: MH_Initialize failed");
        return false;
    }

    // Initialize Path and Config
    char dllPath[MAX_PATH] = {};
    if (GetModuleFileNameA(g_hModule, dllPath, MAX_PATH)) {
        std::string baseDir = std::string(dllPath);
        baseDir = baseDir.substr(0, baseDir.find_last_of("\\/"));
        ConfigManager::GetInstance().Load(baseDir);
    }

    // Create dummy window and swapchain to get vtables
    WNDCLASSEXA wc = { sizeof(WNDCLASSEXA), CS_HREDRAW | CS_VREDRAW, DefWindowProcA, 0, 0, GetModuleHandle(nullptr), NULL, NULL, NULL, NULL, "VRInjectDummy", NULL };
    RegisterClassExA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "Dummy", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, wc.hInstance, NULL);

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 100;
    sd.BufferDesc.Height = 100;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    ID3D11Device* pDevice = nullptr;
    ID3D11DeviceContext* pContext = nullptr;
    IDXGISwapChain* pSwapChain = nullptr;
    D3D_FEATURE_LEVEL featureLevel;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &pSwapChain, &pDevice, &featureLevel, &pContext);
    if (FAILED(hr)) {
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &pSwapChain, &pDevice, &featureLevel, &pContext);
    }
    
    if (FAILED(hr)) {
        DestroyWindow(hwnd);
        UnregisterClassA(wc.lpszClassName, wc.hInstance);
        return false;
    }

    void** pSwapChainVtable = *reinterpret_cast<void***>(pSwapChain);

    void* presentAddress = pSwapChainVtable[8];
    void* resizeBuffersAddress = pSwapChainVtable[13];
    
    void** pDeviceVtable = *reinterpret_cast<void***>(pDevice);
    void* createTexture2DAddress = pDeviceVtable[5];
    
    void** pContextVtable = *reinterpret_cast<void***>(pContext);
    void* mapAddress = pContextVtable[14];
    void* unmapAddress = pContextVtable[15];
    void* omSetRenderTargetsAddress = pContextVtable[33];
    void* clearDepthStencilViewAddress = pContextVtable[53];
    void* updateSubresourceAddress = pContextVtable[114];
    
    void* present1Address = nullptr;
    IDXGISwapChain1* pSwapChain1 = nullptr;
    if (SUCCEEDED(pSwapChain->QueryInterface(__uuidof(IDXGISwapChain1), (void**)&pSwapChain1))) {
        void** pSwapChain1Vtable = *reinterpret_cast<void***>(pSwapChain1);
        present1Address = pSwapChain1Vtable[22];
        pSwapChain1->Release();
    }

    pSwapChain->Release();
    pContext->Release();
    pDevice->Release();
    DestroyWindow(hwnd);
    UnregisterClassA(wc.lpszClassName, wc.hInstance);

    auto IsValidHookTarget = [](void* addr) -> bool {
        if (!addr) return false;
        HMODULE hMod = nullptr;
        if (!GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(addr), &hMod) || !hMod) {
            LOG_WARN("DX11Hook: Hook target %p has no owning module — likely a proxy vtable, skipping.", addr);
            return false;
        }
        return true;
    };

    bool hookedAny = false;

    if (IsValidHookTarget(presentAddress)) {
        if (MH_CreateHook(presentAddress, (void*)hkPresent, (void**)&OriginalPresent) == MH_OK) {
            g_targetPresent = presentAddress;
            MH_EnableHook(presentAddress);
            hookedAny = true;
        }
    }

    if (IsValidHookTarget(present1Address)) {
        if (MH_CreateHook(present1Address, (void*)hkPresent1, (void**)&OriginalPresent1) == MH_OK) {
            g_targetPresent1 = present1Address;
            MH_EnableHook(present1Address);
            hookedAny = true;
        }
    }

    if (IsValidHookTarget(resizeBuffersAddress)) {
        if (MH_CreateHook(resizeBuffersAddress, (void*)hkResizeBuffers, (void**)&OriginalResizeBuffers) == MH_OK) {
            g_targetResizeBuffers = resizeBuffersAddress;
            MH_EnableHook(resizeBuffersAddress);
        }
    }

    if (IsValidHookTarget(updateSubresourceAddress)) {
        if (MH_CreateHook(updateSubresourceAddress, (void*)hkUpdateSubresource, (void**)&OriginalUpdateSubresource) == MH_OK) {
            g_targetUpdateSubresource = updateSubresourceAddress;
            MH_EnableHook(updateSubresourceAddress);
        }
    }
    
    if (IsValidHookTarget(mapAddress)) {
        if (MH_CreateHook(mapAddress, (void*)hkMap, (void**)&OriginalMap) == MH_OK) {
            g_targetMap = mapAddress;
            MH_EnableHook(mapAddress);
        }
    }
    
    if (IsValidHookTarget(omSetRenderTargetsAddress)) {
        if (MH_CreateHook(omSetRenderTargetsAddress, (void*)hkOMSetRenderTargets, (void**)&OriginalOMSetRenderTargets) == MH_OK) {
            g_targetOMSetRenderTargets = omSetRenderTargetsAddress;
            MH_EnableHook(omSetRenderTargetsAddress);
        }
    }

    if (IsValidHookTarget(clearDepthStencilViewAddress)) {
        if (MH_CreateHook(clearDepthStencilViewAddress, (void*)hkClearDepthStencilView, (void**)&OriginalClearDepthStencilView) == MH_OK) {
            g_targetClearDepthStencilView = clearDepthStencilViewAddress;
            MH_EnableHook(clearDepthStencilViewAddress);
        }
    }

    if (IsValidHookTarget(createTexture2DAddress)) {
        if (MH_CreateHook(createTexture2DAddress, (void*)hkCreateTexture2D, (void**)&OriginalCreateTexture2D) == MH_OK) {
            g_targetCreateTexture2D = createTexture2DAddress;
            MH_EnableHook(createTexture2DAddress);
        }
    }

    return hookedAny;
}

void Shutdown() {
    Dx11LifecycleManager::Get().Shutdown();

    if (g_targetPresent) MH_DisableHook(g_targetPresent);
    if (g_targetPresent1) MH_DisableHook(g_targetPresent1);
    if (g_targetResizeBuffers) MH_DisableHook(g_targetResizeBuffers);
    if (g_targetUpdateSubresource) MH_DisableHook(g_targetUpdateSubresource);
    if (g_targetMap) MH_DisableHook(g_targetMap);
    if (g_targetOMSetRenderTargets) MH_DisableHook(g_targetOMSetRenderTargets);
    if (g_targetClearDepthStencilView) MH_DisableHook(g_targetClearDepthStencilView);
    if (g_targetCreateTexture2D) MH_DisableHook(g_targetCreateTexture2D);

    MH_Uninitialize();
}

} // namespace DX11Hook
} // namespace vrinject
