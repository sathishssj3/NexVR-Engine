#pragma once

#include <windows.h>
#include <xinput.h>

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

    // Used by OpenXRManager to inject 1:1 motion aiming
    void InjectAimDelta(float pitchDeg, float yawDeg);
    void SetOpenXRManager(OpenXRManager* mgr) { m_openxrManager = mgr; }

private:
    InputHook() = default;
    ~InputHook() = default;

    XINPUT_STATE m_emulatedState = {};
    bool m_initialized = false;
    bool m_usesRawInput = false;
    OpenXRManager* m_openxrManager = nullptr;

    // Hook definitions
    static DWORD WINAPI HookedXInputGetState(DWORD dwUserIndex, XINPUT_STATE* pState);
    static DWORD WINAPI HookedXInputSetState(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration);

    // Trampolines
    typedef DWORD (WINAPI *XInputGetState_t)(DWORD, XINPUT_STATE*);
    typedef DWORD (WINAPI *XInputSetState_t)(DWORD, XINPUT_VIBRATION*);
    
    XInputGetState_t OriginalXInputGetState = nullptr;
    XInputSetState_t OriginalXInputSetState = nullptr;
};

} // namespace vrinject
