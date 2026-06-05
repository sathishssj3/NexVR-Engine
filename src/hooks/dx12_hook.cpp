#include "dx12_hook.h"
#include "../core/logger.h"
#include "MinHook.h"
#include <mutex>

#include "../ai_matrix_classifier/matrix_classifier.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

namespace vrinject {
namespace DX12Hook {

typedef HRESULT(__stdcall* Present_t)(IDXGISwapChain*, UINT, UINT);
typedef void(__stdcall* ExecuteCommandLists_t)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);
typedef HRESULT(__stdcall* Map_t)(ID3D12Resource*, UINT, const D3D12_RANGE*, void**);
typedef void(__stdcall* Unmap_t)(ID3D12Resource*, UINT, const D3D12_RANGE*);

Present_t OriginalPresent = nullptr;
ExecuteCommandLists_t OriginalExecuteCommandLists = nullptr;
Map_t OriginalMap = nullptr;
Unmap_t OriginalUnmap = nullptr;

vrinject::ai::MatrixClassifier g_matrixClassifierDX12;

FrameResourcesDX12 g_frameResources;
std::mutex g_resourceMutex;
OnFrameCallbackDX12 g_onFrameCallback = nullptr;

HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    std::lock_guard<std::mutex> lock(g_resourceMutex);

    if (g_frameResources.swapChain != pSwapChain) {
        g_frameResources.swapChain = pSwapChain;
        pSwapChain->GetDevice(__uuidof(ID3D12Device), (void**)&g_frameResources.device);
        
        DXGI_SWAP_CHAIN_DESC desc;
        pSwapChain->GetDesc(&desc);
        g_frameResources.width = desc.BufferDesc.Width;
        g_frameResources.height = desc.BufferDesc.Height;
    }

    g_frameResources.valid = (g_frameResources.device != nullptr && g_frameResources.commandQueue != nullptr);

    if (g_onFrameCallback && g_frameResources.valid) {
        g_onFrameCallback(g_frameResources);
    }

    return OriginalPresent(pSwapChain, SyncInterval, Flags);
}

void __stdcall hkExecuteCommandLists(ID3D12CommandQueue* pCommandQueue, UINT NumCommandLists, ID3D12CommandList* const* ppCommandLists) {
    std::lock_guard<std::mutex> lock(g_resourceMutex);
    
    if (pCommandQueue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
        g_frameResources.commandQueue = pCommandQueue;
    }

    OriginalExecuteCommandLists(pCommandQueue, NumCommandLists, ppCommandLists);
}

HRESULT __stdcall hkMap(ID3D12Resource* pResource, UINT Subresource, const D3D12_RANGE* pReadRange, void** ppData) {
    HRESULT hr = OriginalMap(pResource, Subresource, pReadRange, ppData);
    if (SUCCEEDED(hr) && ppData && *ppData) {
        // We defer scanning until Unmap, or we can scan right now if we intercept it for stereoscopic modification.
        // Actually, we must scan the buffer data to find the projection matrix and intercept it!
        // We will store the pointer and scan it on unmap, or pass it to our classifier.
    }
    return hr;
}

void __stdcall hkUnmap(ID3D12Resource* pResource, UINT Subresource, const D3D12_RANGE* pWrittenRange) {
    // In a real DX12 hook, we would track mapped resources. Since we are just prototyping,
    // we assume the game wrote to the buffer just before unmapping.
    // D3D12 resource sizes aren't trivially available without GetDesc(), so we use a safe scan size.
    D3D12_RESOURCE_DESC desc = pResource->GetDesc();
    if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
        D3D12_RANGE readRange = {0, 0};
        void* pData = nullptr;
        // Temporary re-map to read what the game just wrote before it hits the GPU
        if (SUCCEEDED(OriginalMap(pResource, Subresource, &readRange, &pData)) && pData) {
            auto detections = g_matrixClassifierDX12.ScanBuffer(pData, desc.Width);
            if (!detections.empty()) {
                // LOG_INFO("DX12: Found %zu projection matrices in buffer", detections.size());
            }
            OriginalUnmap(pResource, Subresource, nullptr);
        }
    }
    OriginalUnmap(pResource, Subresource, pWrittenRange);
}

bool Initialize() {
    if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED) return false;

    // Create dummy window and swapchain to get vtables
    WNDCLASSEXA wc = { sizeof(WNDCLASSEXA), CS_HREDRAW | CS_VREDRAW, DefWindowProcA, 0, 0, GetModuleHandle(nullptr), NULL, NULL, NULL, NULL, "VRInjectDummyDX12", NULL };
    RegisterClassExA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "Dummy", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, wc.hInstance, NULL);

    ID3D12Device* pDevice = nullptr;
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void**)&pDevice))) {
        DestroyWindow(hwnd);
        UnregisterClassA(wc.lpszClassName, wc.hInstance);
        return false;
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    ID3D12CommandQueue* pCommandQueue = nullptr;
    pDevice->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), (void**)&pCommandQueue);

    IDXGIFactory4* dxgiFactory = nullptr;
    CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));

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
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    IDXGISwapChain* pSwapChain = nullptr;
    dxgiFactory->CreateSwapChain(pCommandQueue, &sd, &pSwapChain);

    void** pSwapChainVtable = *reinterpret_cast<void***>(pSwapChain);
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
    pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource), (void**)&pDummyResource);

    void* presentAddress = pSwapChainVtable[8];
    void* executeCommandListsAddress = pCommandQueueVtable[10];
    
    void* mapAddress = nullptr;
    void* unmapAddress = nullptr;
    if (pDummyResource) {
        void** pResourceVtable = *reinterpret_cast<void***>(pDummyResource);
        mapAddress = pResourceVtable[8];
        unmapAddress = pResourceVtable[9];
        pDummyResource->Release();
    }

    pSwapChain->Release();
    dxgiFactory->Release();
    pCommandQueue->Release();
    pDevice->Release();
    DestroyWindow(hwnd);
    UnregisterClassA(wc.lpszClassName, wc.hInstance);

    if (MH_CreateHook(presentAddress, (void*)hkPresent, (void**)&OriginalPresent) != MH_OK) {
        LOG_ERROR("MH_CreateHook failed for DX12 Present");
        return false;
    }
    if (MH_CreateHook(executeCommandListsAddress, (void*)hkExecuteCommandLists, (void**)&OriginalExecuteCommandLists) != MH_OK) {
        LOG_ERROR("MH_CreateHook failed for DX12 ExecuteCommandLists");
        return false;
    }
    if (mapAddress && MH_CreateHook(mapAddress, (void*)hkMap, (void**)&OriginalMap) != MH_OK) {
        LOG_ERROR("MH_CreateHook failed for DX12 Map");
        return false;
    }
    if (unmapAddress && MH_CreateHook(unmapAddress, (void*)hkUnmap, (void**)&OriginalUnmap) != MH_OK) {
        LOG_ERROR("MH_CreateHook failed for DX12 Unmap");
        return false;
    }
    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        LOG_ERROR("MH_EnableHook failed for DX12");
        return false;
    }

    return true;
}

void Shutdown() {
    MH_DisableHook(MH_ALL_HOOKS);
    std::lock_guard<std::mutex> lock(g_resourceMutex);
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
