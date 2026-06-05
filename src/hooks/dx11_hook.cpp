#include "dx11_hook.h"
#include "MinHook.h"
#include <mutex>

#include <string>
#include "../rendering/stereo_pipeline.h"
#include "../core/logger.h"
#include "../core/config_manager.h"
#include "../ai_matrix_classifier/matrix_classifier.h"

#include <string>
#include <chrono>
#include <utility>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include "../rendering/backends/dx11_renderer.h"
#include "../rendering/backends/dx12_renderer.h"
#include "../hooks/input_hook.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static WNDPROC g_oWndProc = nullptr;
static HWND g_gameHwnd = nullptr;
static bool g_imguiInitialized = false;

LRESULT CALLBACK HookedWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (g_imguiInitialized && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
        return true;
        
    if (g_imguiInitialized) {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse && (uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST)) return true;
        if (io.WantCaptureKeyboard && (uMsg >= WM_KEYFIRST && uMsg <= WM_KEYLAST)) return true;
    }
    
    if (g_oWndProc) return CallWindowProc(g_oWndProc, hWnd, uMsg, wParam, lParam);
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

extern HMODULE g_hModule; // Declared in dllmain.cpp

namespace vrinject {
namespace DX11Hook {

typedef HRESULT(__stdcall* Present_t)(IDXGISwapChain*, UINT, UINT);
typedef void(__stdcall* OMSetRenderTargets_t)(ID3D11DeviceContext*, UINT, ID3D11RenderTargetView* const*, ID3D11DepthStencilView*);

Present_t OriginalPresent = nullptr;
OMSetRenderTargets_t OriginalOMSetRenderTargets = nullptr;

FrameResources g_frameResources;
std::mutex g_resourceMutex;
OnFrameCallback g_onFrameCallback = nullptr;

// Per-frame tracking
Microsoft::WRL::ComPtr<ID3D11DepthStencilView> g_largestDSVThisFrame;
UINT g_maxDepthPixels = 0;
bool g_firstFrame = true;

StereoPipeline g_stereoPipeline;
bool g_pipelineInitialized = false;

// GameSense AI Matrix Classifier
vrinject::ai::MatrixClassifier g_matrixClassifier;
bool g_classifierInitialized = false;

// Sub-Phase 7B: Global Renderers
DX11Renderer g_dx11Renderer;
DX12Renderer g_dx12Renderer;
static std::once_flag s_initFlag;
GraphicsAPI g_currentAPI = GraphicsAPI::UNKNOWN;

void InitializeBackend(IDXGISwapChain* pSwapChain) {
    // Detect API by querying the swapchain device
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11Device;
    if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&d3d11Device))) {
        g_currentAPI = GraphicsAPI::DX11;
        ID3D11DeviceContext* context = nullptr;
        d3d11Device->GetImmediateContext(&context);
        
        g_dx11Renderer.Initialize(d3d11Device.Get(), context);
        g_stereoPipeline.GetOpenXRManager()->SetRenderer(&g_dx11Renderer);
        LOG_INFO("DX11 backend initialized via IRenderer");
        context->Release();
    } else {
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> d3d12Queue;
        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D12CommandQueue), (void**)&d3d12Queue))) {
            g_currentAPI = GraphicsAPI::DX12;
            
            Microsoft::WRL::ComPtr<ID3D12Device> d3d12Device;
            d3d12Queue->GetDevice(__uuidof(ID3D12Device), (void**)&d3d12Device);

            g_dx12Renderer.Initialize(d3d12Device.Get(), d3d12Queue.Get());
            g_stereoPipeline.GetOpenXRManager()->SetRenderer(&g_dx12Renderer);
            LOG_INFO("DX12 backend initialized via IRenderer");
        }
    }
}

// Caching and throttling state to prevent CPU-GPU stalls and ONNX overhead
static bool s_projectionFound = false;
static int s_projectionBufferIndex = -1;
static size_t s_projectionOffset = 0;
static int s_scanFrameThrottle = 0;
static Microsoft::WRL::ComPtr<ID3D11Buffer> s_cachedStagingBuffer;
static UINT s_cachedStagingSize = 0;

void ScanActiveConstantBuffers(ID3D11DeviceContext* context, StereoParams& params) {
    if (!g_classifierInitialized) return;

    ID3D11Buffer* boundCBs[4] = { nullptr };
    context->VSGetConstantBuffers(0, 4, boundCBs);

    // If projection matrix slot & offset has been found previously, read it directly and skip ONNX!
    if (s_projectionFound && s_projectionBufferIndex >= 0) {
        ID3D11Buffer* cb = boundCBs[s_projectionBufferIndex];
        if (cb) {
            D3D11_BUFFER_DESC desc;
            cb->GetDesc(&desc);

            if (desc.ByteWidth >= s_projectionOffset + 64) {
                // Ensure staging buffer matches the width
                if (!s_cachedStagingBuffer || s_cachedStagingSize < desc.ByteWidth) {
                    D3D11_BUFFER_DESC stagingDesc = {};
                    stagingDesc.ByteWidth = desc.ByteWidth;
                    stagingDesc.Usage = D3D11_USAGE_STAGING;
                    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                    stagingDesc.BindFlags = 0;
                    stagingDesc.MiscFlags = 0;
                    stagingDesc.StructureByteStride = 0;

                    Microsoft::WRL::ComPtr<ID3D11Device> device;
                    context->GetDevice(&device);
                    s_cachedStagingBuffer.Reset();
                    if (SUCCEEDED(device->CreateBuffer(&stagingDesc, nullptr, &s_cachedStagingBuffer))) {
                        s_cachedStagingSize = desc.ByteWidth;
                    }
                }

                if (s_cachedStagingBuffer) {
                    context->CopyResource(s_cachedStagingBuffer.Get(), cb);

                    D3D11_MAPPED_SUBRESOURCE mapped;
                    // Read staging buffer asynchronously without CPU stall if possible
                    HRESULT hr = context->Map(s_cachedStagingBuffer.Get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped);
                    if (SUCCEEDED(hr)) {
                        const float* mat = reinterpret_cast<const float*>(reinterpret_cast<const char*>(mapped.pData) + s_projectionOffset);
                        float xScale = mat[0];
                        float yScale = mat[5];
                        float A = mat[10];
                        float B = mat[14];

                        if (xScale > 0.0f && yScale > 0.0f && std::abs(A) > 0.001f) {
                            float nearP = -B / A;
                            float farP = -B / (A - 1.0f);
                            if (nearP > 0.001f && farP > nearP && farP < 100000.0f) {
                                params.nearPlane = nearP;
                                params.farPlane = farP;
                            }
                        }
                        context->Unmap(s_cachedStagingBuffer.Get(), 0);
                    }
                }
            }
        }

        for (int i = 0; i < 4; ++i) {
            if (boundCBs[i]) boundCBs[i]->Release();
        }
        return;
    }

    // Throttle the full ONNX scan to run once every 120 frames (2 seconds at 60fps)
    if (s_scanFrameThrottle++ % 120 != 0) {
        for (int i = 0; i < 4; ++i) {
            if (boundCBs[i]) boundCBs[i]->Release();
        }
        return;
    }

    for (int i = 0; i < 4; ++i) {
        if (!boundCBs[i]) continue;

        D3D11_BUFFER_DESC desc;
        boundCBs[i]->GetDesc(&desc);

        if (desc.ByteWidth >= 64) {
            // Allocate staging buffer (cached on subsequent throttled frames if matching width)
            if (!s_cachedStagingBuffer || s_cachedStagingSize < desc.ByteWidth) {
                D3D11_BUFFER_DESC stagingDesc = {};
                stagingDesc.ByteWidth = desc.ByteWidth;
                stagingDesc.Usage = D3D11_USAGE_STAGING;
                stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                stagingDesc.BindFlags = 0;
                stagingDesc.MiscFlags = 0;
                stagingDesc.StructureByteStride = 0;

                Microsoft::WRL::ComPtr<ID3D11Device> device;
                context->GetDevice(&device);
                s_cachedStagingBuffer.Reset();
                if (SUCCEEDED(device->CreateBuffer(&stagingDesc, nullptr, &s_cachedStagingBuffer))) {
                    s_cachedStagingSize = desc.ByteWidth;
                }
            }

            if (s_cachedStagingBuffer) {
                context->CopyResource(s_cachedStagingBuffer.Get(), boundCBs[i]);

                D3D11_MAPPED_SUBRESOURCE mapped;
                // Since this is a throttled scan, we can block to ensure we successfully classify it
                if (SUCCEEDED(context->Map(s_cachedStagingBuffer.Get(), 0, D3D11_MAP_READ, 0, &mapped))) {
                    auto detections = g_matrixClassifier.ScanBuffer(mapped.pData, desc.ByteWidth);

                    for (const auto& result : detections) {
                        int offset = result.first;
                        const auto& pred = result.second;

                        if (pred.type == ai::MatrixType::PerspectiveProjection && pred.confidence > 0.85f) {
                            const float* mat = reinterpret_cast<const float*>(reinterpret_cast<const char*>(mapped.pData) + offset);
                            
                            float xScale = mat[0];
                            float yScale = mat[5];
                            float A = mat[10];
                            float B = mat[14];

                            if (xScale > 0.0f && yScale > 0.0f && std::abs(A) > 0.001f) {
                                float nearP = -B / A;
                                float farP = -B / (A - 1.0f);
                                float aspect = yScale / xScale;

                                if (nearP > 0.001f && farP > nearP && farP < 100000.0f && aspect > 0.1f && aspect < 10.0f) {
                                    params.nearPlane = nearP;
                                    params.farPlane = farP;
                                    
                                    // Cache the findings to permanently disable ONNX scanning
                                    s_projectionFound = true;
                                    s_projectionBufferIndex = i;
                                    s_projectionOffset = offset;

                                    LOG_INFO("GameSense AI: Active Projection Matrix detected & cached! Slot: %d | Offset: %zu | Near: %.3f | Far: %.1f (Confidence: %.1f%%)", 
                                        i, offset, nearP, farP, pred.confidence * 100.0f);
                                    break;
                                }
                            }
                        }
                    }
                    context->Unmap(s_cachedStagingBuffer.Get(), 0);
                    if (s_projectionFound) {
                        break; // Stop scanning other buffers if found
                    }
                }
            }
        }
    }

    for (int i = 0; i < 4; ++i) {
        if (boundCBs[i]) boundCBs[i]->Release();
    }
}

HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    std::unique_lock<std::mutex> lock(g_resourceMutex);

    if (g_frameResources.swapChain != pSwapChain) {
        if (g_frameResources.device) g_frameResources.device->Release();
        if (g_frameResources.context) g_frameResources.context->Release();

        pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_frameResources.device);
        g_frameResources.device->GetImmediateContext(&g_frameResources.context);
        g_frameResources.swapChain = pSwapChain;
        
        g_pipelineInitialized = false;
        g_stereoPipeline.Shutdown();
    }

    std::call_once(s_initFlag, [&]() {
        InitializeBackend(pSwapChain);
    });

    if (g_currentAPI != GraphicsAPI::DX11) {
        // Prototype currently relies on DX11 for the rest of the stereopipeline (stereo_pipeline.cpp).
        // For Phase 7B, we just verify that DX12 is detected and initialized, and then bail out of the DX11-specific capture logic.
        return OriginalPresent(pSwapChain, SyncInterval, Flags);
    }

    // Capture color buffer (backbuffer)
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    if (SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer))) {
        D3D11_TEXTURE2D_DESC desc;
        backBuffer->GetDesc(&desc);

        if (g_frameResources.width != desc.Width || g_frameResources.height != desc.Height) {
            g_frameResources.width = desc.Width;
            g_frameResources.height = desc.Height;
            g_frameResources.colorBuffer.Reset();
            g_frameResources.colorSRV.Reset();
            g_pipelineInitialized = false; // Re-init on resize
            g_stereoPipeline.Shutdown();
        }

        if (!g_frameResources.colorBuffer) {
            D3D11_TEXTURE2D_DESC stagingDesc = desc;
            stagingDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            stagingDesc.Usage = D3D11_USAGE_DEFAULT;
            stagingDesc.CPUAccessFlags = 0;
            if (SUCCEEDED(g_frameResources.device->CreateTexture2D(&stagingDesc, nullptr, &g_frameResources.colorBuffer))) {
                g_frameResources.device->CreateShaderResourceView(g_frameResources.colorBuffer.Get(), nullptr, &g_frameResources.colorSRV);
            }
        }

        if (g_frameResources.colorBuffer) {
            g_frameResources.context->CopyResource(g_frameResources.colorBuffer.Get(), backBuffer.Get());
        }
    }

    // Capture depth buffer if one was found this frame
    if (g_largestDSVThisFrame) {
        Microsoft::WRL::ComPtr<ID3D11Resource> depthRes;
        g_largestDSVThisFrame->GetResource(&depthRes);

        Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTex;
        if (SUCCEEDED(depthRes.As(&depthTex))) {
            D3D11_TEXTURE2D_DESC desc;
            depthTex->GetDesc(&desc);

            if (!g_frameResources.depthBuffer || g_frameResources.depthFormat != desc.Format) {
                g_frameResources.depthFormat = desc.Format;
                
                D3D11_TEXTURE2D_DESC stagingDesc = desc;
                stagingDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                stagingDesc.Usage = D3D11_USAGE_DEFAULT;
                stagingDesc.CPUAccessFlags = 0;
                
                // Convert depth format to typeless for SRV creation
                DXGI_FORMAT srvFormat = DXGI_FORMAT_UNKNOWN;
                if (desc.Format == DXGI_FORMAT_D32_FLOAT || desc.Format == DXGI_FORMAT_R32_TYPELESS) {
                    stagingDesc.Format = DXGI_FORMAT_R32_TYPELESS;
                    srvFormat = DXGI_FORMAT_R32_FLOAT;
                } else if (desc.Format == DXGI_FORMAT_D24_UNORM_S8_UINT || desc.Format == DXGI_FORMAT_R24G8_TYPELESS) {
                    stagingDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
                    srvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
                } else if (desc.Format == DXGI_FORMAT_D16_UNORM || desc.Format == DXGI_FORMAT_R16_TYPELESS) {
                    stagingDesc.Format = DXGI_FORMAT_R16_TYPELESS;
                    srvFormat = DXGI_FORMAT_R16_UNORM;
                } else if (desc.Format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT || desc.Format == DXGI_FORMAT_R32G8X24_TYPELESS) {
                    stagingDesc.Format = DXGI_FORMAT_R32G8X24_TYPELESS;
                    srvFormat = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
                }

                if (srvFormat != DXGI_FORMAT_UNKNOWN) {
                    if (SUCCEEDED(g_frameResources.device->CreateTexture2D(&stagingDesc, nullptr, &g_frameResources.depthBuffer))) {
                        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                        srvDesc.Format = srvFormat;
                        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                        srvDesc.Texture2D.MipLevels = 1;
                        g_frameResources.device->CreateShaderResourceView(g_frameResources.depthBuffer.Get(), &srvDesc, &g_frameResources.depthSRV);
                    }
                }
            }

            if (g_frameResources.depthBuffer) {
                g_frameResources.context->CopyResource(g_frameResources.depthBuffer.Get(), depthTex.Get());
            }
        }
    }

    // We only STRICTLY require the color buffer to render the stereo view.
    // If the game is on a 2D menu (like the title screen), it might not bind a depth buffer!
    g_frameResources.valid = (g_frameResources.colorSRV != nullptr);

    if (g_onFrameCallback && g_frameResources.valid) {
        g_onFrameCallback(g_frameResources);
    }
    
    // --- VR STEREO PIPELINE EXECUTION ---
    if (g_frameResources.valid && backBuffer) {
        if (!g_pipelineInitialized) {
            // Get module path to find shaders and models folders
            char dllPath[MAX_PATH] = {};
            if (GetModuleFileNameA(g_hModule, dllPath, MAX_PATH)) {
                std::string moduleDir = std::string(dllPath);
                moduleDir = moduleDir.substr(0, moduleDir.find_last_of("\\/"));
                
                g_pipelineInitialized = g_stereoPipeline.Initialize(
                    g_frameResources.device, 
                    g_frameResources.width, 
                    g_frameResources.height, 
                    moduleDir
                );

                if (g_pipelineInitialized) {
                    InputHook::GetInstance().SetOpenXRManager(g_stereoPipeline.GetOpenXRManager());
                }

                if (ConfigManager::GetInstance().GetConfig().enableImGuiOverlay) {
                    // Try to get HWND from SwapChain
                    DXGI_SWAP_CHAIN_DESC sd;
                    if (SUCCEEDED(pSwapChain->GetDesc(&sd)) && sd.OutputWindow) {
                        g_gameHwnd = sd.OutputWindow;
                        g_oWndProc = (WNDPROC)SetWindowLongPtrA(g_gameHwnd, GWLP_WNDPROC, (LONG_PTR)HookedWndProc);
                        
                        IMGUI_CHECKVERSION();
                        ImGui::CreateContext();
                        ImGuiIO& io = ImGui::GetIO();
                        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
                        
                        ImGui_ImplWin32_Init(g_gameHwnd);
                        ImGui_ImplDX11_Init(g_frameResources.device, g_frameResources.context);
                        g_imguiInitialized = true;
                    }
                }
            }
        }

        if (g_pipelineInitialized) {
            StereoParams params;
            params.width = g_frameResources.width;
            params.height = g_frameResources.height;
            params.reversedZ = g_frameResources.reversedZ ? 1 : 0;
            
            // Scan bound constant buffers to dynamically update projection parameters via GameSense AI
            ScanActiveConstantBuffers(g_frameResources.context, params);
            
            // Calculate deltaTime between frames
            static auto lastTime = std::chrono::high_resolution_clock::now();
            auto currentTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float> elapsed = currentTime - lastTime;
            lastTime = currentTime;
            
            float deltaTime = elapsed.count();
            // Clamp deltaTime to avoid extreme spikes (e.g. loading screens)
            if (deltaTime < 0.0001f) deltaTime = 0.0001f;
            if (deltaTime > 0.1f) deltaTime = 0.0166f; // default to 60fps
            
            ID3D11DeviceContext* defCtx = g_stereoPipeline.GetDeferredContext();

            // Generate stereoscopic frames!
            g_stereoPipeline.Render(
                defCtx, 
                g_frameResources.context, 
                g_frameResources.colorSRV.Get(), 
                g_frameResources.depthSRV.Get(), 
                params, 
                deltaTime
            );
            
            // --- IMGUI RENDERING ---
            if (g_imguiInitialized && ConfigManager::GetInstance().GetConfig().enableImGuiOverlay) {
                ImGui_ImplDX11_NewFrame();
                ImGui_ImplWin32_NewFrame();
                ImGui::NewFrame();

                VRConfig& cfg = ConfigManager::GetInstance().GetConfigMutable();
                
                ImGui::Begin("VRInject Configuration", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
                ImGui::Text("Stereoscopic Parameters");
                ImGui::Separator();
                if (ImGui::SliderFloat("IPD (Meters)", &cfg.ipd, 0.050f, 0.075f)) {
                    ConfigManager::GetInstance().Save();
                }
                if (ImGui::SliderFloat("Convergence", &cfg.convergence, 1.0f, 100.0f)) {
                    ConfigManager::GetInstance().Save();
                }
                if (ImGui::Checkbox("Enable Neural Inpainter", &cfg.enableNeuralInpainter)) {
                    ConfigManager::GetInstance().Save();
                }
                ImGui::End();

                ImGui::Render();
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            }

            // Create RTV for backbuffer to draw side-by-side directly to screen
            Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferRTV;
            if (SUCCEEDED(g_frameResources.device->CreateRenderTargetView(backBuffer.Get(), nullptr, &backBufferRTV))) {
                g_stereoPipeline.RenderSideBySide(defCtx, backBufferRTV.Get());
            }

            // Execute commands and perfectly restore game's state!
            Microsoft::WRL::ComPtr<ID3D11CommandList> cmdList;
            if (SUCCEEDED(defCtx->FinishCommandList(FALSE, &cmdList))) {
                g_frameResources.context->ExecuteCommandList(cmdList.Get(), TRUE); // TRUE = Restore Context State
                
                // Profiling is finished. Active and stall-free!
                g_stereoPipeline.ReadAndLogProfiling(g_frameResources.context);
            }
        }
    }
    // ------------------------------------

    // Reset tracking for next frame
    g_largestDSVThisFrame.Reset();
    g_maxDepthPixels = 0;
    g_firstFrame = false;

    lock.unlock(); // Release lock before blocking in OriginalPresent

    return OriginalPresent(pSwapChain, SyncInterval, Flags);
}

void __stdcall hkOMSetRenderTargets(ID3D11DeviceContext* pContext, UINT NumViews, ID3D11RenderTargetView* const* ppRenderTargetViews, ID3D11DepthStencilView* pDepthStencilView) {
    if (pDepthStencilView) {
        Microsoft::WRL::ComPtr<ID3D11Resource> res;
        pDepthStencilView->GetResource(&res);

        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
        if (SUCCEEDED(res.As(&tex))) {
            D3D11_TEXTURE2D_DESC desc;
            tex->GetDesc(&desc);

            UINT pixels = desc.Width * desc.Height;
            if (pixels > g_maxDepthPixels) {
                std::lock_guard<std::mutex> lock(g_resourceMutex);
                if (pixels > g_maxDepthPixels) { // Double-checked lock for thread safety
                    g_maxDepthPixels = pixels;
                    g_largestDSVThisFrame = pDepthStencilView;
                }
            }
        }
    }

    OriginalOMSetRenderTargets(pContext, NumViews, ppRenderTargetViews, pDepthStencilView);
}

bool Initialize() {
    if (MH_Initialize() != MH_OK) return false;

    // Initialize GameSense AI Matrix Classifier
    char dllPath[MAX_PATH] = {};
    if (GetModuleFileNameA(g_hModule, dllPath, MAX_PATH)) {
        std::string baseDir = std::string(dllPath);
        baseDir = baseDir.substr(0, baseDir.find_last_of("\\/"));
        
        // Load Configuration
        ConfigManager::GetInstance().Load(baseDir);

        std::string modelPath = baseDir + "\\models\\matrix_classifier.onnx";
        std::wstring wModelPath(modelPath.begin(), modelPath.end());
        g_classifierInitialized = g_matrixClassifier.Initialize(wModelPath);
        if (g_classifierInitialized) {
            LOG_INFO("GameSense AI: Matrix Classifier successfully initialized.");
        } else {
            LOG_WARN("GameSense AI: Failed to initialize Matrix Classifier. Heuristic fallback will be used.");
        }
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

    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &pSwapChain, &pDevice, &featureLevel, &pContext);
    
    if (FAILED(hr)) {
        DestroyWindow(hwnd);
        UnregisterClassA(wc.lpszClassName, wc.hInstance);
        return false;
    }

    void** pSwapChainVtable = *reinterpret_cast<void***>(pSwapChain);
    void** pContextVtable = *reinterpret_cast<void***>(pContext);

    void* presentAddress = pSwapChainVtable[8];
    void* omSetRenderTargetsAddress = pContextVtable[33];

    pSwapChain->Release();
    pContext->Release();
    pDevice->Release();
    DestroyWindow(hwnd);
    UnregisterClassA(wc.lpszClassName, wc.hInstance);

    // Create and enable hooks
    if (MH_CreateHook(presentAddress, (void*)hkPresent, (void**)&OriginalPresent) != MH_OK) {
        LOG_ERROR("MH_CreateHook failed for Present");
        return false;
    }
    if (MH_CreateHook(omSetRenderTargetsAddress, (void*)hkOMSetRenderTargets, (void**)&OriginalOMSetRenderTargets) != MH_OK) {
        LOG_ERROR("MH_CreateHook failed for OMSetRenderTargets");
        return false;
    }
    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        LOG_ERROR("MH_EnableHook failed");
        return false;
    }

    return true;
}

void Shutdown() {
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    
    std::lock_guard<std::mutex> lock(g_resourceMutex);
    g_stereoPipeline.Shutdown();
    g_frameResources = FrameResources();
}

FrameResources GetCurrentFrame() {
    std::lock_guard<std::mutex> lock(g_resourceMutex);
    return g_frameResources;
}

void SetOnFrameCallback(OnFrameCallback callback) {
    g_onFrameCallback = callback;
}

} // namespace DX11Hook
} // namespace vrinject
