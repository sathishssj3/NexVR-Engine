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
Present_t OriginalPresentDX12 = nullptr;
Present1_t OriginalPresent1DX12 = nullptr;

HRESULT __stdcall hkPresentDX12(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    static int s_count = 0;
    if (s_count++ < 10) LOG_INFO("hkPresentDX12 called! pSwapChain=%p", pSwapChain);
    OnPresent(pSwapChain);
    return OriginalPresentDX12(pSwapChain, SyncInterval, Flags);
}

HRESULT __stdcall hkPresent1DX12(IDXGISwapChain1* pSwapChain, UINT SyncInterval, UINT PresentFlags, const DXGI_PRESENT_PARAMETERS* pPresentParameters) {
    static int s_count1 = 0;
    if (s_count1++ < 10) LOG_INFO("hkPresent1DX12 called! pSwapChain=%p", pSwapChain);
    OnPresent(pSwapChain);
    return OriginalPresent1DX12(pSwapChain, SyncInterval, PresentFlags, pPresentParameters);
}

ExecuteCommandLists_t OriginalExecuteCommandLists = nullptr;



FrameResourcesDX12 g_frameResources;
std::mutex g_resourceMutex;
OnFrameCallbackDX12 g_onFrameCallback = nullptr;

DX12Renderer g_dx12Renderer;
OpenXRManager g_openxrManager;
bool g_openxrInitialized = false;

void OnPresent(IDXGISwapChain* pSwapChain) {
    std::lock_guard<std::mutex> lock(g_resourceMutex);

    if (g_frameResources.swapChain != pSwapChain) {
        g_frameResources.swapChain = pSwapChain;
        Microsoft::WRL::ComPtr<ID3D12Device> device;
        if (FAILED(pSwapChain->GetDevice(__uuidof(ID3D12Device), (void**)&device))) {
            LOG_ERROR("DX12Hook: SwapChain->GetDevice(ID3D12Device) failed");
            // In DX12, the SwapChain might return the CommandQueue instead of the Device!
            Microsoft::WRL::ComPtr<ID3D12CommandQueue> swapQueue;
            if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D12CommandQueue), (void**)&swapQueue))) {
                LOG_INFO("DX12Hook: SwapChain returned CommandQueue instead of Device! Fetching Device from Queue.");
                if (!g_frameResources.commandQueue) {
                    g_frameResources.commandQueue = swapQueue.Detach();
                }
                if (FAILED(g_frameResources.commandQueue->GetDevice(__uuidof(ID3D12Device), (void**)&device))) {
                    LOG_ERROR("DX12Hook: CommandQueue->GetDevice failed");
                }
            } else if (g_frameResources.commandQueue) {
                LOG_INFO("DX12Hook: Fetching Device from captured CommandQueue.");
                if (FAILED(g_frameResources.commandQueue->GetDevice(__uuidof(ID3D12Device), (void**)&device))) {
                    LOG_ERROR("DX12Hook: Captured CommandQueue->GetDevice failed");
                }
            }
            
            if (!device) {
                g_frameResources.device = nullptr;
                g_frameResources.valid = false;
                return;
            }
        }
        if (g_frameResources.device) g_frameResources.device->Release();
        g_frameResources.device = device.Detach();
        
        DXGI_SWAP_CHAIN_DESC desc;
        pSwapChain->GetDesc(&desc);
        g_frameResources.width = desc.BufferDesc.Width;
        g_frameResources.height = desc.BufferDesc.Height;
    }

    g_frameResources.valid = (g_frameResources.device != nullptr && g_frameResources.commandQueue != nullptr);

    if (!g_frameResources.commandQueue) {
        g_frameResources.commandQueue = DXGIFactoryHook::GetCapturedCommandQueue();
        g_frameResources.valid = (g_frameResources.device != nullptr && g_frameResources.commandQueue != nullptr);
    }

    if (g_frameResources.valid && !g_openxrInitialized) {
        Microsoft::WRL::ComPtr<ID3D12Resource> backBuffer;
        uint32_t width = 1920;
        uint32_t height = 1080;
        if (SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D12Resource), (void**)&backBuffer))) {
            D3D12_RESOURCE_DESC resDesc = backBuffer->GetDesc();
            width = static_cast<uint32_t>(resDesc.Width);
            height = resDesc.Height;
        }

        g_dx12Renderer.Initialize(g_frameResources.device, g_frameResources.commandQueue);
        g_openxrManager.SetRenderer(&g_dx12Renderer);
        if (g_openxrManager.Initialize(GraphicsAPI::DX12, g_frameResources.device, g_frameResources.commandQueue, width, height)) {
            InputHook::GetInstance().SetOpenXRManager(&g_openxrManager);
            g_openxrInitialized = true;
            LOG_INFO("DX12Hook: OpenXR initialized successfully for DX12 (Size: %ux%u)", width, height);
        } else {
            LOG_ERROR("DX12Hook: Failed to initialize OpenXR");
        }
    }

    if (g_openxrInitialized) {
        g_openxrManager.PollEvents();
        
        if (g_openxrManager.IsSessionRunning()) {
            XrFrameState frameState = {XR_TYPE_FRAME_STATE};
            if (g_openxrManager.BeginFrame(frameState)) {
            Microsoft::WRL::ComPtr<ID3D12Resource> backBuffer;
            if (SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D12Resource), (void**)&backBuffer))) {
                TextureHandle tex;
                tex.nativePtr = backBuffer.Get();
                
                D3D12_RESOURCE_DESC resDesc = backBuffer->GetDesc();
                tex.width = static_cast<uint32_t>(resDesc.Width);
                tex.height = resDesc.Height;

                // For now, submit the same 2D backbuffer to both eyes (Virtual Cinema mode)
                g_openxrManager.EndFrame(frameState, tex, tex, {}, nullptr);
            } else {
                g_openxrManager.EndFrame(frameState, {}, {}, {}, nullptr);
            }
            }
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
    std::lock_guard<std::mutex> lock(g_resourceMutex);
    
    if (pCommandQueue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
        g_frameResources.commandQueue = pCommandQueue;
    }

    OriginalExecuteCommandLists(pCommandQueue, NumCommandLists, ppCommandLists);
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
    void* present1Address = pSwapChain1Vtable[8];
    void* present1ExAddress = pSwapChain1Vtable[22];
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

    LOG_INFO("DX12Hook: Dummy Initialize success, waiting for DynamicHook");
    return true;
}

void DynamicHookSwapChain(IDXGISwapChain* pSwapChain) {
    if (!pSwapChain) return;
    
    static std::mutex s_hookMutex;
    std::lock_guard<std::mutex> lock(s_hookMutex);
    
    void** pVtable = *reinterpret_cast<void***>(pSwapChain);
    void* presentAddress = pVtable[8];
    void* present1Address = nullptr;
    
    static void* s_originalPresent0 = nullptr;
    static void* s_originalPresent1 = nullptr;
    
    IDXGISwapChain1* pSwapChain1 = nullptr;
    if (SUCCEEDED(pSwapChain->QueryInterface(__uuidof(IDXGISwapChain1), (void**)&pSwapChain1))) {
        void** pVtable1 = *reinterpret_cast<void***>(pSwapChain1);
        present1Address = pVtable1[22];
    }
    
    LOG_INFO("DX12Hook: DynamicHookSwapChain called with pSwapChain=%p", pSwapChain);
    LOG_INFO("DX12Hook:   presentAddress=%p, hkPresentDX12=%p", presentAddress, (void*)hkPresentDX12);
    LOG_INFO("DX12Hook:   present1Address=%p, hkPresent1DX12=%p", present1Address, (void*)hkPresent1DX12);

    if (presentAddress && presentAddress != (void*)hkPresentDX12) {
        DWORD oldProtect;
        if (VirtualProtect(&pVtable[8], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
            s_originalPresent0 = pVtable[8];
            OriginalPresentDX12 = (Present_t)s_originalPresent0;
            pVtable[8] = (void*)hkPresentDX12;
            VirtualProtect(&pVtable[8], sizeof(void*), oldProtect, &oldProtect);
            LOG_INFO("DX12Hook: VTable hooked Present");
        } else {
            LOG_ERROR("DX12Hook: VirtualProtect failed for Present");
        }
    }
    
    if (present1Address && present1Address != presentAddress && present1Address != (void*)hkPresent1DX12) {
        if (pSwapChain1) {
            void** pVtable1 = *reinterpret_cast<void***>(pSwapChain1);
            DWORD oldProtect;
            if (VirtualProtect(&pVtable1[22], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
                s_originalPresent1 = pVtable1[22];
                OriginalPresent1DX12 = (Present1_t)s_originalPresent1;
                pVtable1[22] = (void*)hkPresent1DX12;
                VirtualProtect(&pVtable1[22], sizeof(void*), oldProtect, &oldProtect);
                LOG_INFO("DX12Hook: VTable hooked Present1");
            } else {
                LOG_ERROR("DX12Hook: VirtualProtect failed for Present1");
            }
        }
    }
    
    if (pSwapChain1) pSwapChain1->Release();
}

void Shutdown() {
    MH_DisableHook(MH_ALL_HOOKS);
    std::lock_guard<std::mutex> lock(g_resourceMutex);
    if (g_frameResources.device) {
        g_frameResources.device->Release();
    }
    g_frameResources = FrameResourcesDX12();
}

FrameResourcesDX12 GetCurrentFrame() {
    std::lock_guard<std::mutex> lock(g_resourceMutex);
    return g_frameResources;
}

void SetOnFrameCallback(OnFrameCallbackDX12 callback) {
    g_onFrameCallback = callback;
}

} // namespace DX12Hook
} // namespace vrinject
