#include "overlay_manager.h"
#include "logger.h"
#include "config_manager.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"

// Forward declare Win32 message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace vrinject {

void OverlayManager::Initialize(HWND hwnd) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) return;

    m_hwnd = hwnd;
    
    // Create ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    
    // Enable Keyboard Controls and Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    
    // Force ImGui to draw its own software mouse cursor, since we can't see the hardware cursor in VR
    io.MouseDrawCursor = true;
    
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    
    // Make the UI look a bit more "VR-friendly" (larger, higher contrast)
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(1.5f);
    style.Colors[ImGuiCol_WindowBg].w = 0.9f;

    // Initialize Win32 backend
    if (!ImGui_ImplWin32_Init(m_hwnd)) {
        LOG_ERROR("OverlayManager: ImGui_ImplWin32_Init failed!");
        return;
    }

    m_initialized = true;
    LOG_INFO("OverlayManager: Initialized successfully.");
}

bool OverlayManager::HandleWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!m_initialized) return false;

    // Always pass messages to ImGui so it can track mouse movement/keyboard state
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) {
        return true;
    }

    // If the overlay is visible and ImGui wants capture, block the game from seeing it
    if (m_isVisible) {
        ImGuiIO& io = ImGui::GetIO();
        bool isMouseMsg = (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST);
        bool isKeyMsg = (msg >= WM_KEYFIRST && msg <= WM_KEYLAST);

        if (io.WantCaptureMouse && isMouseMsg) return true;
        if (io.WantCaptureKeyboard && isKeyMsg) return true;
    }

    return false;
}

void OverlayManager::Render() {
    if (!m_initialized || !m_isVisible) return;

    auto& cfgManager = ConfigManager::GetInstance();
    auto& cfg = cfgManager.GetConfigMutable();

    // Start the Dear ImGui frame (backend new frames are called in dx11/dx12 hooks)
    ImGui::NewFrame();

    ImGui::SetNextWindowSize(ImVec2(500, 350), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("NexVR Settings", &m_isVisible, ImGuiWindowFlags_AlwaysAutoResize)) {
        
        ImGui::Text("Stereoscopic 3D Settings");
        ImGui::Separator();
        
        float ipd = cfg.ipd;
        if (ImGui::SliderFloat("IPD (Eye Distance)", &ipd, 0.01f, 0.15f, "%.3f m")) {
            cfg.ipd = ipd;
            cfgManager.Save();
        }

        float conv = cfg.convergence;
        if (ImGui::SliderFloat("Convergence", &conv, 1.0f, 1000.0f, "%.1f cm")) {
            cfg.convergence = conv;
            cfgManager.Save();
        }

        ImGui::Spacing();
        ImGui::Text("Performance & Quality");
        ImGui::Separator();

        bool inpainter = cfg.enableNeuralInpainter;
        if (ImGui::Checkbox("Enable Neural Inpainter", &inpainter)) {
            cfg.enableNeuralInpainter = inpainter;
            cfgManager.Save();
        }

        ImGui::Spacing();
        if (ImGui::Button("Close Menu")) {
            m_isVisible = false;
        }
    }
    ImGui::End();

    ImGui::Render();
}

} // namespace vrinject
