#pragma once

#include <windows.h>
#include <xinput.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace vrinject {

class OpenXRManager;

class InputHook {
public:
    static InputHook& GetInstance() {
        static InputHook instance;
        return instance;
    }

    bool Initialize();
    void Shutdown();

    // Used by OpenXRManager to update the global emulated state
    void UpdateEmulatedState(const XINPUT_STATE& state);
    void SetVRControllersActive(bool active) { m_vrControllersActive = active; }

    // Used by OpenXRManager to inject 1:1 motion aiming
    void InjectAimDelta(float pitchDeg, float yawDeg);
    void SetOpenXRManager(OpenXRManager* mgr) { m_openxrManager = mgr; }

    // Start background input capture for Null Driver users
    void StartBackgroundCapture();
    void StopBackgroundCapture();
    HWND GetTargetHwnd() const { return m_targetHwnd; }
    bool IsCaptureActive() const { return m_captureActive; }
    
    int GetVirtualCursorX() const { return m_virtualCursorX.load(); }
    int GetVirtualCursorY() const { return m_virtualCursorY.load(); }

private:
    InputHook() = default;
    ~InputHook() = default;

    XINPUT_STATE m_emulatedState = {};
    bool m_initialized = false;
    bool m_usesRawInput = false;
    bool m_vrControllersActive = false;
    OpenXRManager* m_openxrManager = nullptr;

    // Hook definitions
    static DWORD WINAPI HookedXInputGetState(DWORD dwUserIndex, XINPUT_STATE* pState);
    static DWORD WINAPI HookedXInputSetState(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration);

    // Trampolines
    typedef DWORD (WINAPI *XInputGetState_t)(DWORD, XINPUT_STATE*);
    typedef DWORD (WINAPI *XInputSetState_t)(DWORD, XINPUT_VIBRATION*);
    
    XInputGetState_t OriginalXInputGetState = nullptr;
    XInputSetState_t OriginalXInputSetState = nullptr;

    // Background Capture Support
    std::thread m_captureThread;
    std::atomic<bool> m_captureRunning{false};
    std::atomic<bool> m_captureActive{false}; // Toggled via F12
    std::atomic<bool> m_captureThreadReady{false};
    std::mutex m_captureMutex;
    std::condition_variable m_captureCv;
    HWND m_targetHwnd = nullptr;
    HHOOK m_keyboardHook = nullptr;
    HHOOK m_mouseHook = nullptr;

public:
    std::atomic<int> m_mouseDeltaX{0};
    std::atomic<int> m_mouseDeltaY{0};
    std::atomic<int> m_mouseButtonFlags{0};
    std::atomic<int> m_mouseWheel{0};
    std::atomic<int> m_virtualCursorX{0};
    std::atomic<int> m_virtualCursorY{0};
    std::atomic<bool> m_gameCursorVisible{true};

private:
    void CaptureThreadLoop();
    void FindTargetWindow();
    void ToggleRawInputSink(bool enable);

    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);
};

} // namespace vrinject
