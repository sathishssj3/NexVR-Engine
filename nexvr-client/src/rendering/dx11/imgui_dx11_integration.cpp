#include "rendering/dx11/imgui_dx11_integration.h"
#include "core/logger.h"
#include "core/overlay_manager.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"

namespace vrinject {

bool ImGuiDX11Integration::Initialize(ID3D11Device* device, ID3D11DeviceContext* context) {
    if (m_initialized) return true;
    if (!device || !context) return false;
    if (!ImGui::GetCurrentContext()) return false;

    if (!ImGui_ImplDX11_Init(device, context)) {
        LOG_ERROR("ImGuiDX11: ImGui_ImplDX11_Init failed!");
        return false;
    }

    m_initialized = true;
    LOG_INFO("ImGuiDX11: Initialized successfully.");
    return true;
}

void ImGuiDX11Integration::Render(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11RenderTargetView* rtv) {
    if (!m_initialized || !context || !rtv) return;

    // Save previous render targets and viewport
    ID3D11RenderTargetView* prevRtv = nullptr;
    ID3D11DepthStencilView* prevDsv = nullptr;
    context->OMGetRenderTargets(1, &prevRtv, &prevDsv);

    UINT numViewports = 1;
    D3D11_VIEWPORT prevViewport;
    context->RSGetViewports(&numViewports, &prevViewport);

    // Bind target RTV
    context->OMSetRenderTargets(1, &rtv, nullptr);

    // Render ImGui
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();

    OverlayManager::GetInstance().Render();

    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    // Restore previous render targets and viewport
    context->OMSetRenderTargets(1, &prevRtv, prevDsv);
    if (numViewports > 0) {
        context->RSSetViewports(1, &prevViewport);
    }

    if (prevRtv) prevRtv->Release();
    if (prevDsv) prevDsv->Release();
}

static ID3D11RenderTargetView* GetOrCreateRtvForTexture(
    ID3D11Device* device,
    ID3D11Texture2D* tex,
    Microsoft::WRL::ComPtr<ID3D11Texture2D>& cachedTex,
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>& cachedRtv
) {
    if (!device || !tex) return nullptr;
    if (cachedTex.Get() != tex || !cachedRtv) {
        cachedTex = tex;
        cachedRtv.Reset();

        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        D3D11_TEXTURE2D_DESC texDesc = {};
        tex->GetDesc(&texDesc);

        DXGI_FORMAT rtvFormat = texDesc.Format;
        if (rtvFormat == DXGI_FORMAT_R8G8B8A8_TYPELESS) {
            rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        } else if (rtvFormat == DXGI_FORMAT_B8G8R8A8_TYPELESS) {
            rtvFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
        } else if (rtvFormat == DXGI_FORMAT_R10G10B10A2_TYPELESS) {
            rtvFormat = DXGI_FORMAT_R10G10B10A2_UNORM;
        } else if (rtvFormat == DXGI_FORMAT_R16G16B16A16_TYPELESS) {
            rtvFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
        }

        rtvDesc.Format = rtvFormat;
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;

        HRESULT hr = device->CreateRenderTargetView(tex, &rtvDesc, &cachedRtv);
        if (FAILED(hr)) {
            static bool s_loggedRtvError = false;
            if (!s_loggedRtvError) {
                LOG_ERROR("ImGuiDX11: Failed to create RTV for target texture (format %u -> %u, hr=0x%X)",
                          texDesc.Format, rtvFormat, hr);
                s_loggedRtvError = true;
            }
            return nullptr;
        }
    }
    return cachedRtv.Get();
}

void ImGuiDX11Integration::Render(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D* leftDest, ID3D11Texture2D* rightDest) {
    if (!m_initialized || !device || !context || !leftDest) return;

    ID3D11RenderTargetView* rtvLeft = GetOrCreateRtvForTexture(device, leftDest, m_cachedTextureLeft, m_cachedRtvLeft);
    if (!rtvLeft) return;

    ID3D11RenderTargetView* rtvRight = nullptr;
    if (rightDest && rightDest != leftDest) {
        rtvRight = GetOrCreateRtvForTexture(device, rightDest, m_cachedTextureRight, m_cachedRtvRight);
    }

    // Save previous render targets and viewport
    ID3D11RenderTargetView* prevRtv = nullptr;
    ID3D11DepthStencilView* prevDsv = nullptr;
    context->OMGetRenderTargets(1, &prevRtv, &prevDsv);

    UINT numViewports = 1;
    D3D11_VIEWPORT prevViewport;
    context->RSGetViewports(&numViewports, &prevViewport);

    // Set viewport matching texture
    D3D11_TEXTURE2D_DESC texDesc = {};
    leftDest->GetDesc(&texDesc);
    D3D11_VIEWPORT vp = {};
    vp.Width = static_cast<float>(texDesc.Width);
    vp.Height = static_cast<float>(texDesc.Height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    context->RSSetViewports(1, &vp);

    // Generate ImGui DrawData EXACTLY ONCE per frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();

    OverlayManager::GetInstance().Render();

    ImDrawData* drawData = ImGui::GetDrawData();
    if (drawData && drawData->TotalVtxCount > 0) {
        // 1. Draw into Left Eye
        context->OMSetRenderTargets(1, &rtvLeft, nullptr);
        ImGui_ImplDX11_RenderDrawData(drawData);

        // 2. Draw into Right Eye (reusing the exact same draw data, zero frame re-generation overhead)
        if (rtvRight) {
            context->OMSetRenderTargets(1, &rtvRight, nullptr);
            ImGui_ImplDX11_RenderDrawData(drawData);
        }
    }

    // Restore previous render targets and viewport
    context->OMSetRenderTargets(1, &prevRtv, prevDsv);
    if (numViewports > 0) {
        context->RSSetViewports(1, &prevViewport);
    }

    if (prevRtv) prevRtv->Release();
    if (prevDsv) prevDsv->Release();
}

void ImGuiDX11Integration::RenderToTexture(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D* targetTexture) {
    Render(device, context, targetTexture, nullptr);
}

void ImGuiDX11Integration::InvalidateDeviceObjects() {
    if (m_initialized) {
        m_cachedRtvLeft.Reset();
        m_cachedTextureLeft.Reset();
        m_cachedRtvRight.Reset();
        m_cachedTextureRight.Reset();
        ImGui_ImplDX11_InvalidateDeviceObjects();
    }
}

void ImGuiDX11Integration::CreateDeviceObjects() {
    if (m_initialized) {
        ImGui_ImplDX11_CreateDeviceObjects();
    }
}

void ImGuiDX11Integration::Shutdown() {
    if (m_initialized) {
        m_cachedRtvLeft.Reset();
        m_cachedTextureLeft.Reset();
        m_cachedRtvRight.Reset();
        m_cachedTextureRight.Reset();
        ImGui_ImplDX11_Shutdown();
        m_initialized = false;
        LOG_INFO("ImGuiDX11: Shutdown completed.");
    }
}

} // namespace vrinject
