#include "core/overlay_manager.h"
#include "core/logger.h"
#include "core/config_manager.h"
#include "core/subsystem_context.h"
#include "imgui.h"
#include "imgui_impl_win32.h"

// Forward declare Win32 message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace vrinject {

static void ApplyNexVRTheme(ImGuiStyle& style, ImGuiIO& io) {
    // 1. Font scaling for VR headsets - crisp, sharp, large and legible
    io.FontGlobalScale = 1.35f;

    // 2. Geometry & Spacing
    style.WindowRounding = 12.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 8.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding = 6.0f;
    style.TabRounding = 8.0f;

    style.WindowBorderSize = 1.5f;
    style.FrameBorderSize = 0.5f;
    style.PopupBorderSize = 1.0f;

    style.WindowPadding = ImVec2(20.0f, 18.0f);
    style.FramePadding = ImVec2(14.0f, 8.0f);
    style.ItemSpacing = ImVec2(14.0f, 10.0f);
    style.ItemInnerSpacing = ImVec2(10.0f, 8.0f);
    style.ScrollbarSize = 18.0f;
    style.GrabMinSize = 16.0f;

    // 3. Cyber Glassmorphism Palette (Deep space obsidian + Vibrant Electric Cyan)
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                  = ImVec4(0.96f, 0.98f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.55f, 0.64f, 0.76f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.06f, 0.08f, 0.13f, 0.94f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.09f, 0.12f, 0.18f, 0.70f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.08f, 0.10f, 0.16f, 0.96f);
    colors[ImGuiCol_Border]                = ImVec4(0.00f, 0.72f, 0.95f, 0.65f); // Vibrant cyan border
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.12f, 0.16f, 0.24f, 0.85f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.16f, 0.24f, 0.36f, 0.95f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.20f, 0.30f, 0.44f, 1.00f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.08f, 0.11f, 0.18f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.10f, 0.15f, 0.25f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.05f, 0.07f, 0.12f, 0.80f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.10f, 0.14f, 0.22f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.06f, 0.08f, 0.12f, 0.60f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.20f, 0.30f, 0.45f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.00f, 0.75f, 1.00f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.00f, 0.85f, 1.00f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(0.00f, 0.85f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.00f, 0.80f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.20f, 0.95f, 1.00f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.11f, 0.28f, 0.46f, 0.85f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.15f, 0.40f, 0.68f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.00f, 0.75f, 1.00f, 1.00f);
    colors[ImGuiCol_Header]                = ImVec4(0.12f, 0.24f, 0.38f, 0.80f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.16f, 0.34f, 0.54f, 0.90f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.00f, 0.68f, 0.92f, 1.00f);
    colors[ImGuiCol_Separator]             = ImVec4(0.20f, 0.30f, 0.45f, 0.60f);
    colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.00f, 0.75f, 1.00f, 0.78f);
    colors[ImGuiCol_SeparatorActive]       = ImVec4(0.00f, 0.85f, 1.00f, 1.00f);
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.09f, 0.14f, 0.22f, 0.90f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.16f, 0.34f, 0.54f, 1.00f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.12f, 0.36f, 0.60f, 1.00f);
    colors[ImGuiCol_TabUnfocused]          = ImVec4(0.08f, 0.11f, 0.18f, 0.85f);
    colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.10f, 0.22f, 0.38f, 0.90f);
}

void OverlayManager::Initialize(HWND hwnd) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) return;

    m_hwnd = hwnd;
    if (!m_hwnd) m_hwnd = GetActiveWindow();
    if (!m_hwnd) m_hwnd = GetForegroundWindow();
    if (!m_hwnd) {
        EnumWindows([](HWND h, LPARAM lp) -> BOOL {
            DWORD pid = 0;
            GetWindowThreadProcessId(h, &pid);
            if (pid == GetCurrentProcessId() && IsWindowVisible(h)) {
                *reinterpret_cast<HWND*>(lp) = h;
                return FALSE;
            }
            return TRUE;
        }, (LPARAM)&m_hwnd);
    }
    
    // Create ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    
    // Enable Keyboard Controls and Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    
    // Apply NexVR futuristic Cyber Glassmorphism theme
    ImGuiStyle& style = ImGui::GetStyle();
    ApplyNexVRTheme(style, io);

    // Initialize Win32 backend
    if (m_hwnd && !ImGui_ImplWin32_Init(m_hwnd)) {
        LOG_ERROR("OverlayManager: ImGui_ImplWin32_Init failed!");
        return;
    }

    m_initialized = true;
    LOG_INFO("OverlayManager: Initialized successfully with HWND %p.", m_hwnd);
}

bool OverlayManager::HandleWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!m_initialized || !m_isVisible) return false;

    // Only process and capture input when the in-headset menu is actually visible
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) {
        return true;
    }

    ImGuiIO& io = ImGui::GetIO();
    bool isMouseMsg = (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST);
    bool isKeyMsg = (msg >= WM_KEYFIRST && msg <= WM_KEYLAST);

    if (io.WantCaptureMouse && isMouseMsg) return true;
    if (io.WantCaptureKeyboard && isKeyMsg) return true;

    return false;
}

void OverlayManager::Render() {
    if (!m_initialized) return;

    // Start Dear ImGui frame
    ImGui::NewFrame();
    
    ImGuiIO& io = ImGui::GetIO();
    RECT clientRect = {};
    if (m_hwnd && GetClientRect(m_hwnd, &clientRect) && (clientRect.right > clientRect.left)) {
        io.DisplaySize = ImVec2((float)(clientRect.right - clientRect.left), (float)(clientRect.bottom - clientRect.top));
    } else if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f) {
        io.DisplaySize = ImVec2(1920.0f, 1080.0f);
    }

    io.MouseDrawCursor = m_isVisible;

    if (!m_isVisible) {
        ImGui::EndFrame();
        return;
    }

    auto cfgManager = SubsystemContext::Get().GetConfig();
    if (!cfgManager) {
        ImGui::EndFrame();
        return;
    }
    auto& cfg = cfgManager->GetConfigMutable();

    // Perfectly center the VR menu in the player's direct field of view
    const float winW = 860.0f;
    const float winH = 680.0f;
    float posX = (io.DisplaySize.x - winW) * 0.5f;
    float posY = (io.DisplaySize.y - winH) * 0.5f;
    if (posX < 10.0f) posX = 10.0f;
    if (posY < 10.0f) posY = 10.0f;

    ImGui::SetNextWindowPos(ImVec2(posX, posY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Always);

    ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

    if (ImGui::Begin("NexVR  |  In-Headset VR Dashboard", &m_isVisible, winFlags)) {
        // Top status badge
        ImGui::TextColored(ImVec4(0.00f, 0.85f, 1.00f, 1.0f), "NexVR Engine Active");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.35f, 0.90f, 0.45f, 1.0f), "  ● Full 3D Stereo VR");
        ImGui::SameLine();
        ImGui::TextDisabled("  |  Press HOME to close");

        ImGui::Spacing();

        if (ImGui::BeginTabBar("NexVRMainTabs")) {
            // TAB 1: 3D Depth & Comfort
            if (ImGui::BeginTabItem("  3D Depth & Comfort  ")) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.00f, 0.85f, 1.00f, 1.0f), "Stereoscopic Depth Controls");
                ImGui::Separator();

                // 1. 3D Pop-out / Depth
                float ipdMm = cfg.ipd * 1000.0f;
                ImGui::Text("3D Depth Separation: %.1f mm", ipdMm);
                if (ImGui::SliderFloat("##ipd", &ipdMm, 45.0f, 85.0f, "%.1f mm")) {
                    cfg.ipd = ipdMm / 1000.0f;
                    cfgManager->Save();
                }
                ImGui::TextDisabled("Controls how intense the 3D effect is. Lower = subtle & relaxing, Higher = deeper 3D pop-out.");

                ImGui::Spacing();

                // 2. Character & World Scale
                float worldScale = cfg.vrScaleFactor;
                ImGui::Text("World & Character Scale: %.0f", worldScale);
                if (ImGui::SliderFloat("##worldscale", &worldScale, 30.0f, 250.0f, "%.0f units")) {
                    cfg.vrScaleFactor = worldScale;
                    cfgManager->Save();
                }
                ImGui::SameLine();
                if (ImGui::Button("Default Size")) {
                    cfg.vrScaleFactor = 100.0f;
                    cfgManager->Save();
                }
                ImGui::TextDisabled("Adjust if characters or castle hallways look too giant or too miniature.");

                ImGui::Spacing();

                // 3. Eye Comfort Distance
                float conv = cfg.convergence;
                ImGui::Text("Zero-Parallax Focus Distance: %.1f meters", conv);
                if (ImGui::SliderFloat("##conv", &conv, 1.0f, 30.0f, "%.1f m")) {
                    cfg.convergence = conv;
                    cfgManager->Save();
                }
                ImGui::TextDisabled("Distance in front of you where objects appear at natural resting depth.");

                ImGui::Spacing();

                // 4. Vibrant Colors
                bool srgb = cfg.srgbCorrection;
                if (ImGui::Checkbox("Vibrant Game Colors & Contrast (sRGB Tonemapping)", &srgb)) {
                    cfg.srgbCorrection = srgb;
                    cfgManager->Save();
                }
                ImGui::TextDisabled("Restores deep rich blacks and authentic lighting matching the original game.");

                ImGui::EndTabItem();
            }

            // TAB 2: Performance & Graphics
            if (ImGui::BeginTabItem("  Graphics & Performance  ")) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.00f, 0.85f, 1.00f, 1.0f), "Resolution & Quality");
                ImGui::Separator();

                float resScale = cfg.resolutionScale;
                ImGui::Text("VR Render Sharpness: %.0f%%", resScale * 100.0f);
                if (ImGui::SliderFloat("##resScale", &resScale, 0.70f, 1.50f, "%.2fx")) {
                    cfg.resolutionScale = resScale;
                    cfgManager->Save();
                }
                ImGui::SameLine();
                if (ImGui::Button("Reset 100%")) {
                    cfg.resolutionScale = 1.0f;
                    cfgManager->Save();
                }
                ImGui::TextDisabled("Adjust render resolution. 100% is balanced, 120%+ is ultra sharp on high-end GPUs.");

                ImGui::Spacing();

                bool inpainter = cfg.enableNeuralInpainter;
                if (ImGui::Checkbox("Smart Edge Smoothing (Disocclusion Inpainting)", &inpainter)) {
                    cfg.enableNeuralInpainter = inpainter;
                    cfgManager->Save();
                }
                ImGui::TextDisabled("Fills newly revealed background pixels during head movement to eliminate edge shimmering.");

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.00f, 0.85f, 1.00f, 1.0f), "Live Headset Telemetry");
                ImGui::Separator();

                float fps = io.Framerate > 0.0f ? io.Framerate : 90.0f;
                float ms = 1000.0f / fps;
                ImGui::TextColored(ImVec4(0.35f, 0.90f, 0.45f, 1.0f), "Framerate: %.1f FPS  |  Frame Time: %.2f ms (Target: 11.1 ms)", fps, ms);
                ImGui::ProgressBar(ms / 11.1f, ImVec2(-1, 24), "Frame Budget");
                ImGui::TextDisabled("DirectX 12 stereo injection pipeline running smoothly on GPU.");

                ImGui::EndTabItem();
            }

            // TAB 3: Controls & Help
            if (ImGui::BeginTabItem("  Controls & Shortcuts  ")) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.00f, 0.85f, 1.00f, 1.0f), "Quick Controls");
                ImGui::Separator();

                ImGui::BulletText("Keyboard: Press HOME to open / close this dashboard.");
                ImGui::BulletText("VR Controllers: Left Controller Menu button (or hold Y / B).");
                ImGui::BulletText("Mouse: Point and click on sliders or buttons directly.");

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::Button("Reset All Settings to Recommended VR Defaults", ImVec2(-1, 42))) {
                    cfg.ipd = 0.064f;
                    cfg.convergence = 10.0f;
                    cfg.vrScaleFactor = 100.0f;
                    cfg.resolutionScale = 1.0f;
                    cfg.srgbCorrection = true;
                    cfg.enableNeuralInpainter = true;
                    cfgManager->Save();
                }
                ImGui::TextDisabled("Restores optimal eye comfort, natural character proportions, and vibrant colors.");

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        // Bottom Action Bar (Pinned and visible from every tab)
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.05f, 0.45f, 0.75f, 0.90f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.08f, 0.58f, 0.95f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.00f, 0.75f, 1.00f, 1.00f));

        if (ImGui::Button("  ▶  Resume Game (Press HOME to close)  ", ImVec2(-1, 48))) {
            m_isVisible = false;
        }

        ImGui::PopStyleColor(3);
    }
    ImGui::End();

    ImGui::Render();
}

} // namespace vrinject
