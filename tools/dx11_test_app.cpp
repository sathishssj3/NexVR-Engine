#include <windows.h>
#include <d3d11.h>
#include <stdio.h>
#include <string>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "user32.lib")

bool g_ultimateStress = false;
int g_stressCycles = 0;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void ParseArgs() {
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return;
    
    for (int i = 0; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"--ultimate-stress") {
            g_ultimateStress = true;
        } else if (arg == L"--cycles" && i + 1 < argc) {
            g_stressCycles = _wtoi(argv[i + 1]);
            i++;
        }
    }
    LocalFree(argv);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    ParseArgs();

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "Dx11TestApp";
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0, "Dx11TestApp", "DX11 VR Test App", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, nullptr, nullptr, hInstance, nullptr);

    if (!hwnd) return -1;
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
    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG; // Memory leak tracking in debug
#endif

    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
        nullptr, 0, D3D11_SDK_VERSION, &sd, &swapchain, &device, &featureLevel, &context);

    if (FAILED(hr)) {
        // Fallback to WARP if hardware fails
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags,
            nullptr, 0, D3D11_SDK_VERSION, &sd, &swapchain, &device, &featureLevel, &context);
        if (FAILED(hr)) return -1;
    }

    ID3D11Texture2D* backBuffer = nullptr;
    swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    
    ID3D11RenderTargetView* rtv = nullptr;
    device->CreateRenderTargetView(backBuffer, nullptr, &rtv);
    backBuffer->Release();

    uint64_t frameCount = 0;
    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            if (g_ultimateStress && g_stressCycles > 0 && frameCount >= g_stressCycles) {
                break; // End stress test
            }

            if (g_ultimateStress) {
                // Periodically resize window to trigger swapchain rebuilds on the injector side
                if (frameCount % 600 == 0) {
                    SetWindowPos(hwnd, nullptr, 0, 0, 1024, 768, SWP_NOMOVE | SWP_NOZORDER);
                } else if (frameCount % 600 == 300) {
                    SetWindowPos(hwnd, nullptr, 0, 0, 800, 600, SWP_NOMOVE | SWP_NOZORDER);
                }
                
                // Other stress cases (device removal etc.) could be added here by intentionally leaking 
                // or doing something bad, but resize and random rendering loads are good for now.
            }

            float color[4] = { 0.1f, 0.2f, (frameCount % 255) / 255.0f, 1.0f }; // Flashing blue/green
            context->ClearRenderTargetView(rtv, color);
            swapchain->Present(1, 0);
            
            if (!g_ultimateStress) {
                Sleep(16); // Simulate ~60fps for normal mode. Uncapped in stress mode!
            }
            frameCount++;
        }
    }

    rtv->Release();
    swapchain->Release();
    context->Release();
    device->Release();
    return 0;
}
