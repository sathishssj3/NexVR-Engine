#include "input_hook.h"
#include "../core/logger.h"
#include "../core/config_manager.h"
#include "../rendering/openxr_manager.h"
#include "MinHook.h"
#include <vector>

namespace vrinject {

bool InputHook::Initialize() {
    if (m_initialized) return true;

    if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED) {
        LOG_ERROR("InputHook: Failed to initialize MinHook.");
        return false;
    }

    // Try hooking XInput 1.4, 1.3, and 9.1.0 as different games use different versions.
    const char* xinputLibs[] = { "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll" };
    bool hooked = false;

    for (const char* lib : xinputLibs) {
        HMODULE hMod = LoadLibraryA(lib);
        if (hMod) {
            void* pGetState = (void*)GetProcAddress(hMod, "XInputGetState");
            void* pSetState = (void*)GetProcAddress(hMod, "XInputSetState");

            if (pGetState) {
                MH_CreateHook(pGetState, reinterpret_cast<LPVOID>(&HookedXInputGetState), reinterpret_cast<void**>(&GetInstance().OriginalXInputGetState));
                MH_EnableHook(pGetState);
                hooked = true;
            }
            if (pSetState) {
                MH_CreateHook(pSetState, reinterpret_cast<LPVOID>(&HookedXInputSetState), reinterpret_cast<void**>(&GetInstance().OriginalXInputSetState));
                MH_EnableHook(pSetState);
            }
            if (hooked) {
                LOG_INFO("InputHook: Successfully hooked %s", lib);
                break;
            }
        }
    }

    if (!hooked) {
        LOG_WARN("InputHook: Failed to hook XInput APIs. Game might not use XInput.");
    }

    // Check if the game registered for Raw Input
    UINT numDevices = 0;
    GetRegisteredRawInputDevices(nullptr, &numDevices, sizeof(RAWINPUTDEVICE));
    std::vector<RAWINPUTDEVICE> devices(numDevices);
    GetRegisteredRawInputDevices(devices.data(), &numDevices, sizeof(RAWINPUTDEVICE));

    for (auto& d : devices) {
        if (d.usUsagePage == 0x01 && d.usUsage == 0x02) { // Generic Desktop, Mouse
            m_usesRawInput = true;
        }
    }

    LOG_INFO("InputHook: Input path: %s", m_usesRawInput ? "Raw Input" : "SendInput");

    m_initialized = true;
    return true;
}

void InputHook::Shutdown() {
    // MH_Uninitialize is typically handled globally, so just clear state
    m_initialized = false;
}

void InputHook::UpdateEmulatedState(const XINPUT_STATE& state) {
    m_emulatedState = state;
}

DWORD WINAPI InputHook::HookedXInputGetState(DWORD dwUserIndex, XINPUT_STATE* pState) {
    InputHook& self = GetInstance();
    
    // We only emulate Player 1 (Index 0)
    if (dwUserIndex == 0 && pState) {
        *pState = self.m_emulatedState;
        return ERROR_SUCCESS;
    }
    
    // Pass through other controllers
    if (self.OriginalXInputGetState) {
        return self.OriginalXInputGetState(dwUserIndex, pState);
    }
    return ERROR_DEVICE_NOT_CONNECTED;
}

DWORD WINAPI InputHook::HookedXInputSetState(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration) {
    InputHook& self = GetInstance();
    
    if (dwUserIndex == 0 && pVibration) {
        if (self.m_openxrManager) {
            float left = (float)pVibration->wLeftMotorSpeed / 65535.0f;
            float right = (float)pVibration->wRightMotorSpeed / 65535.0f;
            self.m_openxrManager->ApplyHapticFeedback(left, right);
        }
        return ERROR_SUCCESS;
    }

    if (self.OriginalXInputSetState) {
        return self.OriginalXInputSetState(dwUserIndex, pVibration);
    }
    return ERROR_DEVICE_NOT_CONNECTED;
}

void InputHook::InjectAimDelta(float pitchDeg, float yawDeg) {
    float sens = ConfigManager::GetInstance().GetConfig().motionAimSensitivity;

        LONG dx = static_cast<LONG>(yawDeg   * sens * 10.0f);
        LONG dy = static_cast<LONG>(pitchDeg * sens * 10.0f);
        if (abs(dx) < 1 && abs(dy) < 1) return;

    if (m_usesRawInput) {
        // Raw Input path: post a synthetic WM_INPUT message
        HWND hwnd = GetForegroundWindow();
        if (hwnd) {
            BYTE buffer[sizeof(RAWINPUT)] = {};
            RAWINPUT* raw = (RAWINPUT*)buffer;
            raw->header.dwType = RIM_TYPEMOUSE;
            raw->header.dwSize = sizeof(RAWINPUT);
            raw->data.mouse.usFlags = MOUSE_MOVE_RELATIVE;
            raw->data.mouse.lLastX = dx;
            raw->data.mouse.lLastY = dy;
            PostMessage(hwnd, WM_INPUT, 0, (LPARAM)buffer);
        }
    } else {
        // Standard SendInput path
        INPUT inp         = {};
        inp.type          = INPUT_MOUSE;
        inp.mi.dwFlags    = MOUSEEVENTF_MOVE;
        inp.mi.dx         = dx;
        inp.mi.dy         = dy;
        SendInput(1, &inp, sizeof(INPUT));
    }
}

} // namespace vrinject
