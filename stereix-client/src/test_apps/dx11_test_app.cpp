#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <iostream>
#include <string>
#include <vector>
#include <chrono>

using namespace Microsoft::WRL;

// Simple CLI parser
bool HasArg(int argc, char** argv, const std::string& arg) {
    for (int i = 1; i < argc; ++i) {
        if (arg == argv[i]) return true;
    }
    return false;
}

int GetArgInt(int argc, char** argv, const std::string& arg, int default_val) {
    for (int i = 1; i < argc - 1; ++i) {
        if (arg == argv[i]) return std::stoi(argv[i + 1]);
    }
    return default_val;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

int main(int argc, char** argv) {
    bool stress = HasArg(argc, argv, "--stress");
    bool cameraStress = HasArg(argc, argv, "--camera-stress");
    bool depthStress = HasArg(argc, argv, "--depth-stress");
    int cycles = GetArgInt(argc, argv, "--cycles", 10000);
    int resizeInterval = GetArgInt(argc, argv, "--resize-interval", 10);
    int fullscreenInterval = GetArgInt(argc, argv, "--fullscreen-interval", 50);
    int swapchainInterval = GetArgInt(argc, argv, "--swapchain-interval", 100);
    int seed = GetArgInt(argc, argv, "--seed", 12345);
    bool ultimateStress = HasArg(argc, argv, "--ultimate-stress");

    if (ultimateStress) {
        stress = true;
        cameraStress = true;
        depthStress = true;
        resizeInterval = 60;
        fullscreenInterval = 120;
        swapchainInterval = 180;
    }

    srand(seed);

    WNDCLASSEXA wc = { sizeof(WNDCLASSEXA), CS_HREDRAW | CS_VREDRAW, WindowProc, 0, 0, GetModuleHandleA(nullptr), NULL, NULL, NULL, NULL, "DX11TestApp", NULL };
    RegisterClassExA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "DX11 Test App", WS_OVERLAPPEDWINDOW, 100, 100, 800, 600, NULL, NULL, wc.hInstance, NULL);
    ShowWindow(hwnd, SW_SHOW);

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swapChain;

    auto CreateDX11 = [&]() {
        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 2;
        sd.BufferDesc.Width = 800;
        sd.BufferDesc.Height = 600;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hwnd;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        D3D_FEATURE_LEVEL featureLevel;
        D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &swapChain, &device, &featureLevel, &context);
    };

    CreateDX11();

    ComPtr<ID3D11Buffer> cameraBuffer;
    if (cameraStress) {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(float) * 16;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        device->CreateBuffer(&desc, nullptr, &cameraBuffer);
    }

    ComPtr<ID3D11Texture2D> depthTexture;
    ComPtr<ID3D11DepthStencilView> depthDSV;
    ComPtr<ID3D11RenderTargetView> rtv;
    if (depthStress) {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = 800;
        desc.Height = 600;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_D32_FLOAT;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        device->CreateTexture2D(&desc, nullptr, &depthTexture);
        device->CreateDepthStencilView(depthTexture.Get(), nullptr, &depthDSV);
        
        ComPtr<ID3D11Texture2D> backBuffer;
        swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
        device->CreateRenderTargetView(backBuffer.Get(), nullptr, &rtv);
    }

    int frames = 0;
    bool running = true;
    
    std::cout << "Starting DX11 loop. Stress mode: " << (stress ? "ON" : "OFF") 
              << ", Camera Stress mode: " << (cameraStress ? "ON" : "OFF") 
              << ", Depth Stress mode: " << (depthStress ? "ON" : "OFF") 
              << ", Ultimate Stress mode: " << (ultimateStress ? "ON" : "OFF") << std::endl;

    while (running && (!stress || frames < cycles || ultimateStress)) {
        if (ultimateStress && frames >= cycles && cycles > 0) {
            break;
        }
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) running = false;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (stress) {
            if (frames % resizeInterval == 0) {
                // Resize
                RECT rect;
                GetWindowRect(hwnd, &rect);
                SetWindowPos(hwnd, NULL, 0, 0, (rect.right - rect.left) + (rand() % 10 - 5), (rect.bottom - rect.top) + (rand() % 10 - 5), SWP_NOMOVE | SWP_NOZORDER);
            }
            if (frames % fullscreenInterval == 0) {
                // Toggle fake fullscreen state just by resizing for now, or actual SetFullscreenState
                BOOL isFullscreen;
                if (swapChain && SUCCEEDED(swapChain->GetFullscreenState(&isFullscreen, nullptr))) {
                    swapChain->SetFullscreenState(!isFullscreen, nullptr);
                }
            }
            if (frames > 0 && frames % swapchainInterval == 0) {
                // Recreate swapchain
                context.Reset();
                device.Reset();
                swapChain.Reset();
                CreateDX11();
                if (depthStress) {
                    depthTexture.Reset();
                    depthDSV.Reset();
                    rtv.Reset();
                    
                    D3D11_TEXTURE2D_DESC desc = {};
                    desc.Width = 800;
                    desc.Height = 600;
                    desc.MipLevels = 1;
                    desc.ArraySize = 1;
                    desc.Format = DXGI_FORMAT_D32_FLOAT;
                    desc.SampleDesc.Count = 1;
                    desc.Usage = D3D11_USAGE_DEFAULT;
                    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
                    device->CreateTexture2D(&desc, nullptr, &depthTexture);
                    device->CreateDepthStencilView(depthTexture.Get(), nullptr, &depthDSV);
                    
                    ComPtr<ID3D11Texture2D> backBuffer;
                    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
                    device->CreateRenderTargetView(backBuffer.Get(), nullptr, &rtv);
                }
            }
        }

        if (cameraStress && cameraBuffer && context) {
            if (ultimateStress && rand() % 100 < 5) {
                // Simulate occasional random invalidation of camera
            } else {
                float matrix[16] = {0};
            // Simulate perspective projection matrix changing slightly
            matrix[0] = 1.0f;
            matrix[5] = 1.0f;
            matrix[10] = 1.0f;
            matrix[11] = 1.0f; // z to w
            matrix[15] = 0.0f;
            
            // Adding a small motion delta
            matrix[12] = std::sin((float)frames * 0.01f);
            context->UpdateSubresource(cameraBuffer.Get(), 0, nullptr, matrix, 0, 0);
            }
        }

        if (depthStress && depthDSV && rtv) {
            context->ClearDepthStencilView(depthDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
            ID3D11RenderTargetView* rtvs[] = { rtv.Get() };
            context->OMSetRenderTargets(1, rtvs, depthDSV.Get());
        }

        if (swapChain) {
            swapChain->Present(0, 0);
        }
        
        frames++;
        if (frames % 1000 == 0) std::cout << "Processed " << frames << " frames..." << std::endl;
    }

    std::cout << "Test completed normally." << std::endl;
    return 0;
}
