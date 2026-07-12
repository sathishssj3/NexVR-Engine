#pragma once

#include <windows.h>
#include <mutex>

namespace vrinject {

class OverlayManager {
public:
    static OverlayManager& GetInstance() {
        static OverlayManager instance;
        return instance;
    }

    void Initialize(HWND hwnd);
    void Render();
    
    bool IsOverlayVisible() const { return m_isVisible; }
    void ToggleOverlay() { m_isVisible = !m_isVisible; }
    void SetOverlayVisible(bool visible) { m_isVisible = visible; }

    // Win32 Message Handler for ImGui
    // Returns true if ImGui captured the input, false if it should pass to the game
    bool HandleWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    OverlayManager() = default;
    ~OverlayManager() = default;

    bool m_initialized = false;
    bool m_isVisible = true; // Default to true so the user sees it when launching
    HWND m_hwnd = nullptr;
    std::mutex m_mutex;
};

} // namespace vrinject
