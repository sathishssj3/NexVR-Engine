#include "dx11_hook.h"
#include "MinHook.h"
#include <mutex>

#include <string>
#include "../rendering/stereo_pipeline.h"
#include "../core/logger.h"
#include "../core/config_manager.h"
#include "../core/matrix_classifier.h"

#include <string>
#include <chrono>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include "../rendering/backends/dx11_renderer.h"
#include "../rendering/backends/dx12_renderer.h"
#include "../hooks/input_hook.h"
#include "../hooks/dx12_hook.h"

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
    
    if (g_oWndProc) return CallWindowProcW(g_oWndProc, hWnd, uMsg, wParam, lParam);
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

extern HMODULE g_hModule; // Declared in dllmain.cpp

namespace vrinject {
namespace DX11Hook {

typedef HRESULT(__stdcall* Present_t)(IDXGISwapChain*, UINT, UINT);
typedef HRESULT(__stdcall* Present1_t)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
typedef void(__stdcall* OMSetRenderTargets_t)(ID3D11DeviceContext*, UINT, ID3D11RenderTargetView* const*, ID3D11DepthStencilView*);
typedef HRESULT(__stdcall* Map_t)(ID3D11DeviceContext*, ID3D11Resource*, UINT, D3D11_MAP, UINT, D3D11_MAPPED_SUBRESOURCE*);
typedef void(__stdcall* UpdateSubresource_t)(ID3D11DeviceContext*, ID3D11Resource*, UINT, const D3D11_BOX*, const void*, UINT, UINT);

typedef void(__stdcall* DrawIndexed_t)(ID3D11DeviceContext*, UINT, UINT, INT);

Present_t OriginalPresent = nullptr;
Present1_t OriginalPresent1 = nullptr;
OMSetRenderTargets_t OriginalOMSetRenderTargets = nullptr;
Map_t Original_Map = nullptr;
UpdateSubresource_t Original_UpdateSubresource = nullptr;
DrawIndexed_t OriginalDrawIndexed = nullptr;

FrameResources g_frameResources;
std::recursive_mutex g_resourceMutex;
OnFrameCallback g_onFrameCallback = nullptr;

// Per-frame tracking
Microsoft::WRL::ComPtr<ID3D11DepthStencilView> g_largestDSVThisFrame;
UINT g_maxDepthPixels = 0;
bool g_firstFrame = true;

StereoPipeline g_stereoPipeline;
bool g_pipelineInitialized = false;

// Matrix Classifier
vrinject::MatrixClassifier& g_matrixClassifier = vrinject::MatrixClassifier::Get();
bool g_classifierInitialized = true;

// Sub-Phase 7B: Global Renderers
DX11Renderer g_dx11Renderer;
DX12Renderer g_dx12Renderer;
static bool s_backendInitialized = false;
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


template<typename T, typename OriginalFunc, typename... Args>
HRESULT ProcessPresent(T* pSwapChain, OriginalFunc originalFunc, Args... args) {
    std::unique_lock<std::recursive_mutex> lock(g_resourceMutex);

    if (g_frameResources.swapChain != pSwapChain) {
        if (g_frameResources.device) g_frameResources.device->Release();
        if (g_frameResources.context) g_frameResources.context->Release();

        // FATAL CRASH FIX: When the swapchain/device changes (e.g. during a scene transition or exit to main menu),
        // we MUST destroy all captured textures. Otherwise, we will attempt to call CopyResource
        // between a texture from the OLD device and a texture from the NEW device, instantly crashing the GPU.
        g_frameResources.colorBuffer.Reset();
        g_frameResources.colorSRV.Reset();
        g_frameResources.depthBuffer.Reset();
        g_frameResources.depthSRV.Reset();
        g_frameResources.width = 0;
        g_frameResources.height = 0;
        g_frameResources.depthWidth = 0;
        g_frameResources.depthHeight = 0;
        g_frameResources.depthFormat = DXGI_FORMAT_UNKNOWN;
        g_frameResources.valid = false;
        
        g_matrixClassifier.Reset();
        g_largestDSVThisFrame.Reset();
        g_maxDepthPixels = 0;

        HRESULT deviceHr = pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_frameResources.device);
        if (FAILED(deviceHr) || !g_frameResources.device) {
            s_backendInitialized = false;
            
            // It might be a DX12 swapchain! Check if we can get a DX12 device or queue.
            Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue;
            if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D12CommandQueue), (void**)&queue))) {
                g_currentAPI = GraphicsAPI::DX12;
            } else {
                g_currentAPI = GraphicsAPI::UNKNOWN;
            }
            
            lock.unlock();
            
            if (g_currentAPI == GraphicsAPI::DX12) {
                DX12Hook::OnPresent(pSwapChain);
            }
            
            return originalFunc(pSwapChain, args...);
        }
        g_frameResources.device->GetImmediateContext(&g_frameResources.context);
        if (!g_frameResources.context) {
            g_frameResources.device->Release();
            g_frameResources.device = nullptr;
            s_backendInitialized = false;
            g_currentAPI = GraphicsAPI::UNKNOWN;
            lock.unlock();
            return originalFunc(pSwapChain, args...);
        }
        g_frameResources.swapChain = pSwapChain;
        
        s_backendInitialized = false; // Force backend reinitialization!
        g_pipelineInitialized = false;
        g_stereoPipeline.Shutdown();
    }

    if (!s_backendInitialized) {
        InitializeBackend(pSwapChain);
        s_backendInitialized = true;
    }

    if (g_currentAPI != GraphicsAPI::DX11) {
        if (g_currentAPI == GraphicsAPI::DX12) {
            DX12Hook::OnPresent(pSwapChain);
        }
        // Prototype currently relies on DX11 for the rest of the stereopipeline (stereo_pipeline.cpp).
        // For Phase 7B, we just verify that DX12 is detected and initialized, and then bail out of the DX11-specific capture logic.
        return originalFunc(pSwapChain, args...);
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
            g_frameResources.initAttempted = false;
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

            if (!g_frameResources.depthBuffer || g_frameResources.depthFormat != desc.Format ||
                g_frameResources.depthWidth != desc.Width || g_frameResources.depthHeight != desc.Height) {
                
                g_frameResources.depthFormat = desc.Format;
                g_frameResources.depthWidth = desc.Width;
                g_frameResources.depthHeight = desc.Height;
                g_frameResources.depthSRV.Reset();
                g_frameResources.depthBuffer.Reset();
                
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
                if (g_frameResources.colorSRV && !g_frameResources.initAttempted) {
                    if (g_frameResources.width > 0 && g_frameResources.height > 0) {
                        g_pipelineInitialized = g_stereoPipeline.Initialize(
                            g_frameResources.device, 
                            g_frameResources.width, 
                            g_frameResources.height, 
                            moduleDir
                        );
                        g_frameResources.initAttempted = g_pipelineInitialized;
                        if (!g_pipelineInitialized) {
                            LOG_ERROR("Failed to initialize VR Stereo Pipeline. Disabling VR for this SwapChain.");
                        }
                    }
                }
                
                if (g_pipelineInitialized) {
                    InputHook::GetInstance().SetOpenXRManager(g_stereoPipeline.GetOpenXRManager());
                }

                if (ConfigManager::GetInstance().GetConfig().enableImGuiOverlay) {
                    // Try to get HWND from SwapChain
                    DXGI_SWAP_CHAIN_DESC sd;
                    if (SUCCEEDED(pSwapChain->GetDesc(&sd)) && sd.OutputWindow) {
                        g_gameHwnd = sd.OutputWindow;
                        g_oWndProc = (WNDPROC)SetWindowLongPtrW(g_gameHwnd, GWLP_WNDPROC, (LONG_PTR)HookedWndProc);
                        
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
            
            // Matrix classifier runs asynchronously on Map/UpdateSubresource hooks now
            
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

            // Prevent target game from rendering to the monitor (Monitor Blanking)
            // We clear the backbuffer to solid black before presenting
            Microsoft::WRL::ComPtr<ID3D11RenderTargetView> clearRTV;
            if (SUCCEEDED(g_frameResources.device->CreateRenderTargetView(backBuffer.Get(), nullptr, &clearRTV))) {
                const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
                g_frameResources.context->ClearRenderTargetView(clearRTV.Get(), clearColor);
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

    return originalFunc(pSwapChain, args...);
}


HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    return ProcessPresent(pSwapChain, OriginalPresent, SyncInterval, Flags);
}

HRESULT __stdcall hkPresent1(IDXGISwapChain1* pSwapChain, UINT SyncInterval, UINT PresentFlags, const DXGI_PRESENT_PARAMETERS* pPresentParameters) {
    return ProcessPresent(pSwapChain, OriginalPresent1, SyncInterval, PresentFlags, pPresentParameters);
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
            
            // FATAL CRASH FIX: Shadow maps are often massive (e.g., 4096x4096 or 8192x8192).
            // If we select a shadow map, we feed a mismatching texture size into ONNX DirectML 
            // inside NeuralInpainter, which instantly causes a hard freeze and driver crash.
            // We must filter out depth buffers that are excessively larger than the backbuffer.
            UINT maxAllowedPixels = g_frameResources.width * g_frameResources.height * 2; // Allow up to 2x supersampling
            
            if (pixels > g_maxDepthPixels && pixels <= maxAllowedPixels) {
                std::lock_guard<std::recursive_mutex> lock(g_resourceMutex);
                if (pixels > g_maxDepthPixels) { // Double-checked lock for thread safety
                    g_maxDepthPixels = pixels;
                    g_largestDSVThisFrame = pDepthStencilView;
                }
            }
        }
    }

    OriginalOMSetRenderTargets(pContext, NumViews, ppRenderTargetViews, pDepthStencilView);
}

void __stdcall hkDrawIndexed(ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation) {
    if (IndexCount > 1000) { // arbitrary threshold to filter out tiny UI quads
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> dsv;
        pContext->OMGetRenderTargets(0, nullptr, &dsv);
        if (dsv) {
            Microsoft::WRL::ComPtr<ID3D11Resource> res;
            dsv->GetResource(&res);
            Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
            if (SUCCEEDED(res.As(&tex))) {
                D3D11_TEXTURE2D_DESC desc;
                tex->GetDesc(&desc);
                UINT pixels = desc.Width * desc.Height;
                UINT maxAllowedPixels = g_frameResources.width * g_frameResources.height * 2;
                if (pixels > g_maxDepthPixels && pixels <= maxAllowedPixels) {
                    std::lock_guard<std::recursive_mutex> lock(g_resourceMutex);
                    if (pixels > g_maxDepthPixels) {
                        g_maxDepthPixels = pixels;
                        g_largestDSVThisFrame = dsv;
                    }
                }
            }
        }
    }
    OriginalDrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);
}

// Hook ID3D11DeviceContext::Map
HRESULT STDMETHODCALLTYPE Hooked_Map(
    ID3D11DeviceContext* pCtx,
    ID3D11Resource*      pResource,
    UINT                 Subresource,
    D3D11_MAP            MapType,
    UINT                 MapFlags,
    D3D11_MAPPED_SUBRESOURCE* pMappedResource)
{
    HRESULT hr = Original_Map(pCtx, pResource, Subresource, MapType, MapFlags, pMappedResource);
    if (SUCCEEDED(hr) && pMappedResource && pMappedResource->pData && MapType == D3D11_MAP_WRITE_DISCARD) {
        D3D11_RESOURCE_DIMENSION dim;
        pResource->GetType(&dim);
        if (dim == D3D11_RESOURCE_DIMENSION_BUFFER) {
            D3D11_BUFFER_DESC desc = {};
            static_cast<ID3D11Buffer*>(pResource)->GetDesc(&desc);
            if (desc.BindFlags & D3D11_BIND_CONSTANT_BUFFER) {
                g_matrixClassifier.OnConstantBufferUpdate(pMappedResource->pData, desc.ByteWidth, pMappedResource->pData);
            }
        }
    }
    return hr;
}

// Hook ID3D11DeviceContext::UpdateSubresource
void STDMETHODCALLTYPE Hooked_UpdateSubresource(
    ID3D11DeviceContext* pCtx,
    ID3D11Resource*      pDstResource,
    UINT                 DstSubresource,
    const D3D11_BOX*     pDstBox,
    const void*          pSrcData,
    UINT                 SrcRowPitch,
    UINT                 SrcDepthPitch)
{
    if (pSrcData) {
        D3D11_RESOURCE_DIMENSION dim;
        pDstResource->GetType(&dim);
        if (dim == D3D11_RESOURCE_DIMENSION_BUFFER) {
            D3D11_BUFFER_DESC desc = {};
            static_cast<ID3D11Buffer*>(pDstResource)->GetDesc(&desc);
            if (desc.BindFlags & D3D11_BIND_CONSTANT_BUFFER) {
                g_matrixClassifier.OnConstantBufferUpdate(pSrcData, desc.ByteWidth, const_cast<void*>(pSrcData));
            }
        }
    }
    Original_UpdateSubresource(pCtx, pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);
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
        
        // Load Configuration
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
    void* mapAddress = pContextVtable[14];
    void* updateSubresourceAddress = pContextVtable[16];
    void* drawIndexedAddress = pContextVtable[12];

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

    // Create and enable hooks
    if (MH_CreateHook(presentAddress, (void*)hkPresent, (void**)&OriginalPresent) != MH_OK) {
        LOG_ERROR("MH_CreateHook failed for Present");
        return false;
    }
    if (present1Address && MH_CreateHook(present1Address, (void*)hkPresent1, (void**)&OriginalPresent1) != MH_OK) {
        LOG_ERROR("MH_CreateHook failed for Present1");
        return false;
    }
    if (MH_CreateHook(omSetRenderTargetsAddress, (void*)hkOMSetRenderTargets, (void**)&OriginalOMSetRenderTargets) != MH_OK) {
        LOG_ERROR("MH_CreateHook failed for OMSetRenderTargets");
        return false;
    }
    if (MH_CreateHook(drawIndexedAddress, (void*)hkDrawIndexed, (void**)&OriginalDrawIndexed) != MH_OK) {
        LOG_ERROR("MH_CreateHook failed for DrawIndexed");
        return false;
    }
    if (MH_CreateHook(mapAddress, (void*)Hooked_Map, (void**)&Original_Map) != MH_OK) {
        LOG_ERROR("MH_CreateHook failed for Map");
        return false;
    }
    if (MH_CreateHook(updateSubresourceAddress, (void*)Hooked_UpdateSubresource, (void**)&Original_UpdateSubresource) != MH_OK) {
        LOG_ERROR("MH_CreateHook failed for UpdateSubresource");
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

    if (g_imguiInitialized) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        g_imguiInitialized = false;
    }

    if (g_oWndProc && g_gameHwnd) {
        SetWindowLongPtrW(g_gameHwnd, GWLP_WNDPROC, (LONG_PTR)g_oWndProc);
        g_oWndProc = nullptr;
    }
    
    std::lock_guard<std::recursive_mutex> lock(g_resourceMutex);
    g_stereoPipeline.Shutdown();
    g_frameResources = FrameResources();
}

FrameResources GetCurrentFrame() {
    std::lock_guard<std::recursive_mutex> lock(g_resourceMutex);
    return g_frameResources;
}

void SetOnFrameCallback(OnFrameCallback callback) {
    g_onFrameCallback = callback;
}

} // namespace DX11Hook
} // namespace vrinject
