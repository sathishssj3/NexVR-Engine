#include <windows.h>
#include <d3d11.h>
#include <stdio.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "user32.lib")

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "DummyD3D11App";
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0, "DummyD3D11App", "Dummy D3D11 VR Target", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, nullptr, nullptr, hInstance, nullptr);

    FILE* logFile;
    fopen_s(&logFile, "dummy_log.txt", "w");
    if (!hwnd) {
        if (logFile) { fprintf(logFile, "CreateWindowEx failed: %lu\n", GetLastError()); fclose(logFile); }
        return -1;
    }
    if (logFile) { fprintf(logFile, "Window created. HWND: %p\n", hwnd); }

    ShowWindow(hwnd, SW_SHOW);

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 800;
    sd.BufferDesc.Height = 600;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* swapchain = nullptr;

    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION, &sd, &swapchain, &device, &featureLevel, &context);

    if (FAILED(hr)) {
        if (logFile) { fprintf(logFile, "D3D11CreateDeviceAndSwapChain failed: %x\n", hr); fclose(logFile); }
        return -1;
    }
    if (logFile) { fprintf(logFile, "D3D11 Device created.\n"); fflush(logFile); }

    ID3D11Texture2D* backBuffer = nullptr;
    swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    
    ID3D11RenderTargetView* rtv = nullptr;
    device->CreateRenderTargetView(backBuffer, nullptr, &rtv);
    backBuffer->Release();

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            float color[4] = { 0.1f, 0.2f, 0.3f, 1.0f };
            context->ClearRenderTargetView(rtv, color);
            swapchain->Present(1, 0);
            Sleep(16); // Simulate ~60fps
        }
    }

    rtv->Release();
    swapchain->Release();
    context->Release();
    device->Release();
    return 0;
}
