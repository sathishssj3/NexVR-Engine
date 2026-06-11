#include "d3d12_hook.h"
#include "dxgi_factory_hook.h"
#include "MinHook.h"
#include "../core/logger.h"
#include "../rendering/openxr_manager.h"
#include "../rendering/backends/dx12_renderer.h"
#include "../hooks/input_hook.h"
#include <mutex>

namespace vrinject {
namespace D3D12Hook {

typedef HRESULT(__stdcall* Present_t)(IDXGISwapChain*, UINT, UINT);
Present_t OriginalPresent = nullptr;

std::mutex g_mutex;
bool g_initialized = false;

DX12Renderer g_renderer;
OpenXRManager g_openxr;
IDXGISwapChain* g_swapChain = nullptr;

HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    std::unique_lock<std::mutex> lock(g_mutex);

    if (g_swapChain != pSwapChain) {
        g_swapChain = pSwapChain;
        g_initialized = false;
        g_openxr.Shutdown();
    }

    if (!g_initialized) {
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue;
        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D12CommandQueue), (void**)&queue))) {
            // Some games pass queue directly
        } else {
            queue = DXGIFactoryHook::GetCapturedCommandQueue();
        }

        if (queue) {
            Microsoft::WRL::ComPtr<ID3D12Device> device;
            queue->GetDevice(__uuidof(ID3D12Device), (void**)&device);

            if (device) {
                g_renderer.Initialize(device.Get(), queue.Get());
                g_openxr.SetRenderer(&g_renderer);
                if (g_openxr.Initialize(GraphicsAPI::DX12, device.Get(), queue.Get())) {
                    InputHook::GetInstance().SetOpenXRManager(&g_openxr);
                    g_initialized = true;
                    LOG_INFO("D3D12Hook: OpenXR initialized successfully for DX12");
                }
            }
        }
    }

    if (g_initialized) {
        XrFrameState frameState = {XR_TYPE_FRAME_STATE};
        if (g_openxr.BeginFrame(frameState)) {
            Microsoft::WRL::ComPtr<ID3D12Resource> backBuffer;
            if (SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D12Resource), (void**)&backBuffer))) {
                TextureHandle tex;
                tex.nativePtr = backBuffer.Get();
                tex.format = DXGI_FORMAT_R8G8B8A8_UNORM;
                
                D3D12_RESOURCE_DESC desc = backBuffer->GetDesc();
                tex.width = desc.Width;
                tex.height = desc.Height;

                // For DX12 prototype, we copy the exact same backbuffer to both eyes
                g_openxr.EndFrame(frameState, tex, tex, {}, nullptr);
            } else {
                g_openxr.EndFrame(frameState, {}, {}, {}, nullptr);
            }
        }
    }

    lock.unlock();
    return OriginalPresent(pSwapChain, SyncInterval, Flags);
}

bool Initialize() {
    WNDCLASSEXA wc = { sizeof(WNDCLASSEXA), CS_HREDRAW | CS_VREDRAW, DefWindowProcA, 0, 0, GetModuleHandle(nullptr), NULL, NULL, NULL, NULL, "VRInjectDummyDX12", NULL };
    RegisterClassExA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "Dummy", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, wc.hInstance, NULL);

    IDXGIFactory2* pFactory = nullptr;
    CreateDXGIFactory1(__uuidof(IDXGIFactory2), (void**)&pFactory);

    ID3D12Device* pDevice = nullptr;
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void**)&pDevice))) {
        DestroyWindow(hwnd);
        UnregisterClassA(wc.lpszClassName, wc.hInstance);
        return false;
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ID3D12CommandQueue* pQueue = nullptr;
    pDevice->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), (void**)&pQueue);

    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.BufferCount = 2;
    sd.Width = 100;
    sd.Height = 100;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.SampleDesc.Count = 1;

    IDXGISwapChain1* pSwapChain = nullptr;
    pFactory->CreateSwapChainForHwnd(pQueue, hwnd, &sd, nullptr, nullptr, &pSwapChain);

    void** pVtable = *reinterpret_cast<void***>(pSwapChain);
    void* presentAddress = pVtable[8];

    pSwapChain->Release();
    pQueue->Release();
    pDevice->Release();
    pFactory->Release();
    DestroyWindow(hwnd);
    UnregisterClassA(wc.lpszClassName, wc.hInstance);

    if (MH_CreateHook(presentAddress, (void*)hkPresent, (void**)&OriginalPresent) != MH_OK) {
        LOG_ERROR("D3D12Hook: Failed to hook Present");
        return false;
    }

    if (MH_EnableHook(presentAddress) != MH_OK) {
        LOG_ERROR("D3D12Hook: Failed to enable hook");
        return false;
    }

    LOG_INFO("D3D12Hook: Initialized successfully");
    return true;
}

void Shutdown() {
    MH_DisableHook(MH_ALL_HOOKS);
    std::lock_guard<std::mutex> lock(g_mutex);
    g_openxr.Shutdown();
    g_initialized = false;
    g_swapChain = nullptr;
}

}
}
