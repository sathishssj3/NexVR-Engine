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

void ImGuiDX11Integration::RenderToTexture(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D* targetTexture) {
    if (!m_initialized || !device || !context || !targetTexture) return;

    if (m_cachedTexture.Get() != targetTexture || !m_cachedRtv) {
        m_cachedTexture = targetTexture;
        m_cachedRtv.Reset();

        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        D3D11_TEXTURE2D_DESC texDesc = {};
        targetTexture->GetDesc(&texDesc);
        
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

        HRESULT hr = device->CreateRenderTargetView(targetTexture, &rtvDesc, &m_cachedRtv);
        if (FAILED(hr)) {
            static bool s_loggedRtvError = false;
            if (!s_loggedRtvError) {
                LOG_ERROR("ImGuiDX11: Failed to create RTV for target texture (format %u -> %u, hr=0x%X)", 
                          texDesc.Format, rtvFormat, hr);
                s_loggedRtvError = true;
            }
            return;
        }
    }

    // Set viewport matching texture
    D3D11_TEXTURE2D_DESC texDesc = {};
    targetTexture->GetDesc(&texDesc);
    D3D11_VIEWPORT vp = {};
    vp.Width = static_cast<float>(texDesc.Width);
    vp.Height = static_cast<float>(texDesc.Height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;

    ID3D11RenderTargetView* prevRtv = nullptr;
    ID3D11DepthStencilView* prevDsv = nullptr;
    context->OMGetRenderTargets(1, &prevRtv, &prevDsv);

    UINT numViewports = 1;
    D3D11_VIEWPORT prevViewport;
    context->RSGetViewports(&numViewports, &prevViewport);

    ID3D11RenderTargetView* rtv = m_cachedRtv.Get();
    context->OMSetRenderTargets(1, &rtv, nullptr);
    context->RSSetViewports(1, &vp);

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();

    OverlayManager::GetInstance().Render();

    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    context->OMSetRenderTargets(1, &prevRtv, prevDsv);
    if (numViewports > 0) {
        context->RSSetViewports(1, &prevViewport);
    }

    if (prevRtv) prevRtv->Release();
    if (prevDsv) prevDsv->Release();
}

void ImGuiDX11Integration::InvalidateDeviceObjects() {
    if (m_initialized) {
        m_cachedRtv.Reset();
        m_cachedTexture.Reset();
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
        m_cachedRtv.Reset();
        m_cachedTexture.Reset();
        ImGui_ImplDX11_Shutdown();
        m_initialized = false;
        LOG_INFO("ImGuiDX11: Shutdown completed.");
    }
}

} // namespace vrinject
