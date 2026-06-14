#include "dx12_hook.h"
#include "dxgi_factory_hook.h"
#include "../core/logger.h"
#include "MinHook.h"
#include "../rendering/openxr_manager.h"
#include "../rendering/backends/dx12_renderer.h"
#include "../hooks/input_hook.h"
#include <mutex>
#include <wrl/client.h>



#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

namespace vrinject {
namespace DX12Hook {

typedef void(__stdcall* ExecuteCommandLists_t)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);
typedef HRESULT(__stdcall* Present_t)(IDXGISwapChain*, UINT, UINT);
typedef HRESULT(__stdcall* Present1_t)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
typedef void(__stdcall* DrawIndexedInstanced_t)(ID3D12GraphicsCommandList*, UINT, UINT, UINT, INT, UINT);

Present_t OriginalPresentDX12 = nullptr;
Present1_t OriginalPresent1DX12 = nullptr;

typedef HRESULT(__stdcall* ResizeBuffers_t)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
ResizeBuffers_t OriginalResizeBuffers = nullptr;

typedef HRESULT(__stdcall* ResizeBuffers1_t)(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT, const UINT*, IUnknown* const*);
ResizeBuffers1_t OriginalResizeBuffers1 = nullptr;

ExecuteCommandLists_t OriginalExecuteCommandLists = nullptr;
DrawIndexedInstanced_t OriginalDrawIndexedInstanced = nullptr;

extern OpenXRManager g_openxrManager;
extern bool g_openxrInitialized;
void OnPresent(IDXGISwapChain* pSwapChain);

HRESULT __stdcall hkPresentDX12(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    OnPresent(pSwapChain);
    // Note: We cannot skip OriginalPresent without starving the game's input message pump.
    return OriginalPresentDX12(pSwapChain, SyncInterval, Flags);
}

HRESULT __stdcall hkPresent1DX12(IDXGISwapChain1* pSwapChain, UINT SyncInterval, UINT PresentFlags, const DXGI_PRESENT_PARAMETERS* pPresentParameters) {
    OnPresent(pSwapChain);
    // Note: We cannot skip OriginalPresent without starving the game's input message pump.
    return OriginalPresent1DX12(pSwapChain, SyncInterval, PresentFlags, pPresentParameters);
}

HRESULT __stdcall hkResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) {
    LOG_INFO("DX12Hook: hkResizeBuffers requested format %d", NewFormat);
    return OriginalResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
}

HRESULT __stdcall hkResizeBuffers1(IDXGISwapChain3* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags, const UINT* pCreationNodeMask, IUnknown* const* ppPresentQueue) {
    LOG_INFO("DX12Hook: hkResizeBuffers1 requested format %d", NewFormat);
    return OriginalResizeBuffers1(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags, pCreationNodeMask, ppPresentQueue);
}

FrameResourcesDX12 g_frameResources;
std::recursive_mutex g_resourceMutex;
OnFrameCallbackDX12 g_onFrameCallback = nullptr;

DX12Renderer g_dx12Renderer;
OpenXRManager g_openxrManager;

bool g_openxrInitialized = false;
IDXGISwapChain* g_mainSwapChain = nullptr;

void OnPresent(IDXGISwapChain* pSwapChain) {
    if (g_mainSwapChain != nullptr && pSwapChain != g_mainSwapChain) {
        return; // Ignore secondary swapchains (UI, Overlays, CEF)
    }

    std::lock_guard<std::recursive_mutex> lock(g_resourceMutex);

    if (!g_frameResources.valid) {
        // Capture device and queue from swapchain
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> localQueue;
        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D12CommandQueue), (void**)&localQueue))) {
            // Direct3D 12 swapchains return the command queue used for presentation!
            g_frameResources.commandQueue = localQueue.Detach();
            
            Microsoft::WRL::ComPtr<ID3D12Device> localDevice;
            if (SUCCEEDED(g_frameResources.commandQueue->GetDevice(__uuidof(ID3D12Device), (void**)&localDevice))) {
                if (g_frameResources.device) {
                    g_frameResources.device->Release();
                }
                g_frameResources.device = localDevice.Detach();
                g_frameResources.valid = true;
            }
        }
    }

    if (g_frameResources.swapChain != pSwapChain) {
        g_frameResources.swapChain = pSwapChain;
        if (g_mainSwapChain == nullptr) {
            g_mainSwapChain = pSwapChain;
        }
        
        // In DX12, IDXGISwapChain::GetDevice returns the ID3D12CommandQueue that was used to create it!
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> swapQueue;
        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D12CommandQueue), (void**)&swapQueue))) {
            if (g_frameResources.commandQueue) {
                g_frameResources.commandQueue->Release();
            }
            g_frameResources.commandQueue = swapQueue.Detach();
            
            // Now get the actual device from the command queue
            Microsoft::WRL::ComPtr<ID3D12Device> localDevice;
            if (SUCCEEDED(g_frameResources.commandQueue->GetDevice(__uuidof(ID3D12Device), (void**)&localDevice))) {
                if (g_frameResources.device) {
                    g_frameResources.device->Release();
                }
                g_frameResources.device = localDevice.Detach();
                g_frameResources.valid = true;
            } else {
                LOG_ERROR("DX12Hook: CommandQueue->GetDevice failed");
            }
        } else {
            // Fallback just in case some weird wrapper returned the Device directly
            Microsoft::WRL::ComPtr<ID3D12Device> localDevice;
            if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D12Device), (void**)&localDevice))) {
                if (g_frameResources.device) {
                    g_frameResources.device->Release();
                }
                g_frameResources.device = localDevice.Detach();
                g_frameResources.valid = true;
            } else {
                LOG_ERROR("DX12Hook: SwapChain->GetDevice failed to return CommandQueue OR Device");
            }
        }
        
        DXGI_SWAP_CHAIN_DESC desc;
        pSwapChain->GetDesc(&desc);
        g_frameResources.width = desc.BufferDesc.Width;
        g_frameResources.height = desc.BufferDesc.Height;
        
        static bool s_formatLogged = false;
        if (!s_formatLogged) {
            LOG_INFO("DX12Hook: SwapChain Format = %d, Resolution = %dx%d", desc.BufferDesc.Format, desc.BufferDesc.Width, desc.BufferDesc.Height);
            s_formatLogged = true;
        }
    }

    g_frameResources.valid = (g_frameResources.device != nullptr && g_frameResources.commandQueue != nullptr);

    if (g_frameResources.valid && g_openxrInitialized) {
        g_dx12Renderer.UpdateGameCommandQueue(g_frameResources.commandQueue);
    }

    if (!g_frameResources.commandQueue) {
        g_frameResources.commandQueue = DXGIFactoryHook::GetCapturedCommandQueue();
        g_frameResources.valid = (g_frameResources.device != nullptr && g_frameResources.commandQueue != nullptr);
    }

    if (g_frameResources.valid && !g_openxrInitialized) {
        Microsoft::WRL::ComPtr<ID3D12Resource> backBuffer;
        uint32_t width = 1920;
        uint32_t height = 1080;
        
        UINT backBufferIndex = 0;
        Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain3;
        if (SUCCEEDED(pSwapChain->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&swapChain3))) {
            backBufferIndex = swapChain3->GetCurrentBackBufferIndex();
        }

        if (SUCCEEDED(pSwapChain->GetBuffer(backBufferIndex, __uuidof(ID3D12Resource), (void**)&backBuffer))) {
            D3D12_RESOURCE_DESC resDesc = backBuffer->GetDesc();
            width = static_cast<uint32_t>(resDesc.Width);
            height = resDesc.Height;
        }

        g_dx12Renderer.Initialize(g_frameResources.device, g_frameResources.commandQueue);
        g_openxrManager.SetRenderer(&g_dx12Renderer);
        
        DXGI_SWAP_CHAIN_DESC desc;
        pSwapChain->GetDesc(&desc);
        
        if (g_openxrManager.Initialize(GraphicsAPI::DX12, g_frameResources.device, g_frameResources.commandQueue, width, height, desc.BufferDesc.Format)) {
            InputHook::GetInstance().SetOpenXRManager(&g_openxrManager);
            g_openxrInitialized = true;
            LOG_INFO("DX12Hook: OpenXR initialized successfully for DX12 (Size: %ux%u, Format: %d)", width, height, desc.BufferDesc.Format);
        } else {
            LOG_ERROR("DX12Hook: Failed to initialize OpenXR");
        }
    }

    if (g_openxrInitialized && g_openxrManager.IsSessionRunning()) {
        Microsoft::WRL::ComPtr<ID3D12Resource> backBuffer;
        UINT backBufferIndex = 0;
        Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain3;
        if (SUCCEEDED(pSwapChain->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&swapChain3))) {
            backBufferIndex = swapChain3->GetCurrentBackBufferIndex();
        }

        if (SUCCEEDED(pSwapChain->GetBuffer(backBufferIndex, __uuidof(ID3D12Resource), (void**)&backBuffer))) {
            TextureHandle tex;
            tex.nativePtr = backBuffer.Get();
            
            D3D12_RESOURCE_DESC resDesc = backBuffer->GetDesc();
            tex.width = static_cast<uint32_t>(resDesc.Width);
            tex.height = resDesc.Height;

            g_dx12Renderer.ExecuteTonemapToIntermediate(tex);
        }
    }

    if (g_onFrameCallback && g_frameResources.valid) {
        // Just once, print that we are rendering
        static bool s_printed = false;
        if (!s_printed) {
            LOG_INFO("DX12Hook: Firing OnFrameCallback! VR should be active now.");
            s_printed = true;
        }
        g_onFrameCallback(g_frameResources);
    } else {
        static int s_failCount = 0;
        if (s_failCount++ < 10) {
            LOG_ERROR("DX12Hook: OnPresent failed to fire callback! callback_valid=%d, resources_valid=%d, device=%p, queue=%p", 
                (bool)g_onFrameCallback, g_frameResources.valid, (void*)g_frameResources.device, (void*)g_frameResources.commandQueue);
        }
    }
}

void __stdcall hkExecuteCommandLists(ID3D12CommandQueue* pCommandQueue, UINT NumCommandLists, ID3D12CommandList* const* ppCommandLists) {
    {
        std::lock_guard<std::recursive_mutex> lock(g_resourceMutex);
        
        // Only capture if we don't have a definitive presentation queue yet
        if (!g_frameResources.commandQueue && pCommandQueue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
            g_frameResources.commandQueue = pCommandQueue;
            // Don't AddRef here because it's a weak pointer in this context,
            // and OnPresent will permanently lock the exact presentation queue shortly.
        }
    }

    OriginalExecuteCommandLists(pCommandQueue, NumCommandLists, ppCommandLists);
}

void __stdcall hkDrawIndexedInstanced(ID3D12GraphicsCommandList* pCommandList, UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation) {
    OriginalDrawIndexedInstanced(pCommandList, IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}



bool Initialize() {
    LOG_INFO("DX12Hook: Initialize started");
    if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED) {
        LOG_ERROR("DX12Hook: MinHook failed");
        return false;
    }

    // Create dummy window and swapchain to get vtables
    WNDCLASSEXA wc = { sizeof(WNDCLASSEXA), CS_HREDRAW | CS_VREDRAW, DefWindowProcA, 0, 0, GetModuleHandle(nullptr), NULL, NULL, NULL, NULL, "VRInjectDummyDX12", NULL };
    RegisterClassExA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "Dummy", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, wc.hInstance, NULL);

    ID3D12Device* pDevice = nullptr;
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void**)&pDevice))) {
        LOG_ERROR("DX12Hook: D3D12CreateDevice failed");
        DestroyWindow(hwnd);
        UnregisterClassA(wc.lpszClassName, wc.hInstance);
        return false;
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    ID3D12CommandQueue* pCommandQueue = nullptr;
    if (FAILED(pDevice->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), (void**)&pCommandQueue)) || !pCommandQueue) {
        LOG_ERROR("DX12Hook: CreateCommandQueue failed");
        pDevice->Release();
        DestroyWindow(hwnd);
        UnregisterClassA(wc.lpszClassName, wc.hInstance);
        return false;
    }

    IDXGIFactory4* dxgiFactory = nullptr;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory))) || !dxgiFactory) {
        LOG_ERROR("DX12Hook: CreateDXGIFactory1 failed");
        pCommandQueue->Release();
        pDevice->Release();
        DestroyWindow(hwnd);
        UnregisterClassA(wc.lpszClassName, wc.hInstance);
        return false;
    }

    DXGI_SWAP_CHAIN_DESC sd0 = {};
    sd0.BufferCount = 2;
    sd0.BufferDesc.Width = 100;
    sd0.BufferDesc.Height = 100;
    sd0.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd0.BufferDesc.RefreshRate.Numerator = 60;
    sd0.BufferDesc.RefreshRate.Denominator = 1;
    sd0.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd0.OutputWindow = hwnd;
    sd0.SampleDesc.Count = 1;
    sd0.Windowed = TRUE;
    sd0.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    IDXGISwapChain* pSwapChain0 = nullptr;
    if (FAILED(dxgiFactory->CreateSwapChain(pCommandQueue, &sd0, &pSwapChain0)) || !pSwapChain0) {
        LOG_ERROR("DX12Hook: CreateSwapChain failed");
        dxgiFactory->Release();
        pCommandQueue->Release();
        pDevice->Release();
        DestroyWindow(hwnd);
        UnregisterClassA(wc.lpszClassName, wc.hInstance);
        return false;
    }

    HWND hwnd2 = CreateWindowExA(0, wc.lpszClassName, "Dummy2", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, wc.hInstance, NULL);

    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.BufferCount = 2;
    sd.Width = 100;
    sd.Height = 100;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.SampleDesc.Count = 1;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    IDXGISwapChain1* pSwapChain1 = nullptr;
    if (FAILED(dxgiFactory->CreateSwapChainForHwnd(pCommandQueue, hwnd2, &sd, nullptr, nullptr, &pSwapChain1)) || !pSwapChain1) {
        LOG_ERROR("DX12Hook: CreateSwapChainForHwnd failed");
        pSwapChain0->Release();
        dxgiFactory->Release();
        pCommandQueue->Release();
        pDevice->Release();
        DestroyWindow(hwnd);
        UnregisterClassA(wc.lpszClassName, wc.hInstance);
        return false;
    }


    void** pSwapChain0Vtable = *reinterpret_cast<void***>(pSwapChain0);
    void** pSwapChain1Vtable = *reinterpret_cast<void***>(pSwapChain1);
    void** pCommandQueueVtable = *reinterpret_cast<void***>(pCommandQueue);

    // Create a committed resource to get the ID3D12Resource vtable
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = 256;
    resDesc.Height = 1;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.Format = DXGI_FORMAT_UNKNOWN;
    resDesc.SampleDesc.Count = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ID3D12Resource* pDummyResource = nullptr;
    if (FAILED(pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource), (void**)&pDummyResource))) {
        pDummyResource = nullptr;
    }

    void* present0Address = pSwapChain0Vtable[8];
    void* resizeBuffersAddress = pSwapChain0Vtable[13];
    void* present1Address = pSwapChain1Vtable[8];
    void* present1ExAddress = pSwapChain1Vtable[22];
    void* resizeBuffers1Address = nullptr;
    
    // Check if IDXGISwapChain3 is supported
    IDXGISwapChain3* pSwapChain3 = nullptr;
    if (pSwapChain1 && SUCCEEDED(pSwapChain1->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&pSwapChain3))) {
        void** pSwapChain3Vtable = *reinterpret_cast<void***>(pSwapChain3);
        resizeBuffers1Address = pSwapChain3Vtable[39];
        pSwapChain3->Release();
    }
    void* executeCommandListsAddress = pCommandQueueVtable[10];
    
    pSwapChain0->Release();
    pSwapChain1->Release();
    dxgiFactory->Release();
    void* mapAddress = nullptr;
    void* unmapAddress = nullptr;
    if (pDummyResource) {
        void** pResourceVtable = *reinterpret_cast<void***>(pDummyResource);
        mapAddress = pResourceVtable[8];
        unmapAddress = pResourceVtable[9];
        pDummyResource->Release();
    }

    // Extract Command List VTable for DrawIndexedInstanced
    void* drawIndexedInstancedAddress = nullptr;
    ID3D12CommandAllocator* pAllocator = nullptr;
    if (SUCCEEDED(pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&pAllocator))) {
        ID3D12GraphicsCommandList* pCommandList = nullptr;
        if (SUCCEEDED(pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pAllocator, nullptr, __uuidof(ID3D12GraphicsCommandList), (void**)&pCommandList))) {
            void** pCommandListVtable = *reinterpret_cast<void***>(pCommandList);
            drawIndexedInstancedAddress = pCommandListVtable[13]; // DrawIndexedInstanced
            pCommandList->Release();
        }
        pAllocator->Release();
    }

    pCommandQueue->Release();
    pDevice->Release();
    DestroyWindow(hwnd);
    DestroyWindow(hwnd2);
    UnregisterClassA(wc.lpszClassName, wc.hInstance);

    if (MH_CreateHook(executeCommandListsAddress, (void*)hkExecuteCommandLists, (void**)&OriginalExecuteCommandLists) != MH_OK) {
        LOG_ERROR("MH_CreateHook failed for DX12 ExecuteCommandLists");
        // ignore error for cmd list since it might fail if already hooked or agile sdk
    }
    MH_EnableHook(executeCommandListsAddress);

    if (drawIndexedInstancedAddress && MH_CreateHook(drawIndexedInstancedAddress, (void*)hkDrawIndexedInstanced, (void**)&OriginalDrawIndexedInstanced) == MH_OK) {
        MH_EnableHook(drawIndexedInstancedAddress);
    } else {
        LOG_ERROR("DX12Hook: MH_CreateHook failed for DrawIndexedInstanced");
    }

    if (present0Address && MH_CreateHook(present0Address, (void*)hkPresentDX12, (void**)&OriginalPresentDX12) == MH_OK) {
        MH_EnableHook(present0Address);
    } else {
        LOG_ERROR("DX12Hook: MH_CreateHook failed for Present");
    }

    if (present1ExAddress && MH_CreateHook(present1ExAddress, (void*)hkPresent1DX12, (void**)&OriginalPresent1DX12) == MH_OK) {
        MH_EnableHook(present1ExAddress);
    } else {
        LOG_ERROR("DX12Hook: MH_CreateHook failed for Present1");
    }

    if (resizeBuffersAddress && MH_CreateHook(resizeBuffersAddress, (void*)hkResizeBuffers, (void**)&OriginalResizeBuffers) == MH_OK) {
        MH_EnableHook(resizeBuffersAddress);
    } else {
        LOG_ERROR("DX12Hook: MH_CreateHook failed for ResizeBuffers");
    }

    if (resizeBuffers1Address && MH_CreateHook(resizeBuffers1Address, (void*)hkResizeBuffers1, (void**)&OriginalResizeBuffers1) == MH_OK) {
        MH_EnableHook(resizeBuffers1Address);
    }

    LOG_INFO("DX12Hook: Dummy Initialize success, waiting for DynamicHook");
    return true;
}

void Shutdown() {
    MH_DisableHook(MH_ALL_HOOKS);
    std::lock_guard<std::recursive_mutex> lock(g_resourceMutex);
    if (g_frameResources.device) {
        g_frameResources.device->Release();
    }
    g_frameResources = FrameResourcesDX12();
}

FrameResourcesDX12 GetCurrentFrame() {
    std::lock_guard<std::recursive_mutex> lock(g_resourceMutex);
    return g_frameResources;
}

void SetOnFrameCallback(OnFrameCallbackDX12 callback) {
    g_onFrameCallback = callback;
}

} // namespace DX12Hook
} // namespace vrinject
