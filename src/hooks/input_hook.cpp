#include "input_hook.h"
#include "../core/logger.h"
#include "../core/config_manager.h"
#include "../rendering/openxr_manager.h"
#include "MinHook.h"
#include <vector>

namespace vrinject {

// Trampolines for focus spoofing
typedef HWND (WINAPI *GetForegroundWindow_t)();
typedef HWND (WINAPI *GetActiveWindow_t)();
typedef UINT (WINAPI *GetRawInputData_t)(HRAWINPUT, UINT, LPVOID, PUINT, UINT);
typedef BOOL (WINAPI *GetCursorPos_t)(LPPOINT);
typedef HCURSOR (WINAPI *SetCursor_t)(HCURSOR);

GetForegroundWindow_t OriginalGetForegroundWindow = nullptr;
GetActiveWindow_t OriginalGetActiveWindow = nullptr;
GetRawInputData_t OriginalGetRawInputData = nullptr;
GetCursorPos_t OriginalGetCursorPos = nullptr;
SetCursor_t OriginalSetCursor = nullptr;

HWND WINAPI HookedGetForegroundWindow() {
    HWND target = InputHook::GetInstance().GetTargetHwnd();
    if (target && InputHook::GetInstance().IsCaptureActive()) return target;
    if (OriginalGetForegroundWindow) return OriginalGetForegroundWindow();
    return nullptr;
}

HWND WINAPI HookedGetActiveWindow() {
    HWND target = InputHook::GetInstance().GetTargetHwnd();
    if (target && InputHook::GetInstance().IsCaptureActive()) return target;
    if (OriginalGetActiveWindow) return OriginalGetActiveWindow();
    return nullptr;
}

UINT WINAPI HookedGetRawInputData(HRAWINPUT hRawInput, UINT uiCommand, LPVOID pData, PUINT pcbSize, UINT cbSizeHeader) {
    if ((uintptr_t)hRawInput == 0xDEADBEEF) {
        if (uiCommand == RID_INPUT) {
            if (pData == nullptr) {
                *pcbSize = sizeof(RAWINPUT);
                return 0;
            }
            if (*pcbSize < sizeof(RAWINPUT)) return (UINT)-1;
            
            RAWINPUT* ri = (RAWINPUT*)pData;
            ri->header.dwType = RIM_TYPEMOUSE;
            ri->header.dwSize = sizeof(RAWINPUT);
            ri->header.hDevice = (HANDLE)1;
            ri->header.wParam = RIM_INPUT;
            ri->data.mouse.usFlags = MOUSE_MOVE_RELATIVE;
            ri->data.mouse.ulButtons = 0;
            ri->data.mouse.ulRawButtons = 0;
            
            ri->data.mouse.lLastX = InputHook::GetInstance().m_mouseDeltaX.exchange(0);
            ri->data.mouse.lLastY = InputHook::GetInstance().m_mouseDeltaY.exchange(0);
            ri->data.mouse.usButtonFlags = InputHook::GetInstance().m_mouseButtonFlags.exchange(0);
            ri->data.mouse.usButtonData = InputHook::GetInstance().m_mouseWheel.exchange(0);
            if (ri->data.mouse.usButtonData != 0) {
                ri->data.mouse.usButtonFlags |= RI_MOUSE_WHEEL;
            }
            return sizeof(RAWINPUT);
        }
        return (UINT)-1;
    }
    if (OriginalGetRawInputData) return OriginalGetRawInputData(hRawInput, uiCommand, pData, pcbSize, cbSizeHeader);
    return (UINT)-1;
}

BOOL WINAPI HookedGetCursorPos(LPPOINT lpPoint) {
    if (InputHook::GetInstance().IsCaptureActive() && InputHook::GetInstance().GetTargetHwnd()) {
        if (InputHook::GetInstance().m_gameCursorVisible) {
            POINT pt;
            pt.x = InputHook::GetInstance().GetVirtualCursorX();
            pt.y = InputHook::GetInstance().GetVirtualCursorY();
            ClientToScreen(InputHook::GetInstance().GetTargetHwnd(), &pt);
            *lpPoint = pt;
            return TRUE;
        } else {
            int screenWidth = GetSystemMetrics(SM_CXSCREEN);
            int screenHeight = GetSystemMetrics(SM_CYSCREEN);
            lpPoint->x = screenWidth / 2;
            lpPoint->y = screenHeight / 2;
            return TRUE;
        }
    }
    if (OriginalGetCursorPos) return OriginalGetCursorPos(lpPoint);
    return FALSE;
}

HCURSOR WINAPI HookedSetCursor(HCURSOR hCursor) {
    if (hCursor == nullptr) {
        InputHook::GetInstance().m_gameCursorVisible = false;
    } else {
        InputHook::GetInstance().m_gameCursorVisible = true;
    }
    if (OriginalSetCursor) return OriginalSetCursor(hCursor);
    return nullptr;
}

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

    // Hook Focus Functions
    HMODULE hUser32 = GetModuleHandleA("user32.dll");
    if (hUser32) {
        void* pGetForeground = (void*)GetProcAddress(hUser32, "GetForegroundWindow");
        void* pGetActive = (void*)GetProcAddress(hUser32, "GetActiveWindow");
        void* pGetRawInput = (void*)GetProcAddress(hUser32, "GetRawInputData");
        void* pGetCursor = (void*)GetProcAddress(hUser32, "GetCursorPos");
        void* pSetCursor = (void*)GetProcAddress(hUser32, "SetCursor");
        if (pGetForeground) {
            MH_CreateHook(pGetForeground, reinterpret_cast<LPVOID>(&HookedGetForegroundWindow), reinterpret_cast<void**>(&OriginalGetForegroundWindow));
            MH_EnableHook(pGetForeground);
        }
        if (pGetActive) {
            MH_CreateHook(pGetActive, reinterpret_cast<LPVOID>(&HookedGetActiveWindow), reinterpret_cast<void**>(&OriginalGetActiveWindow));
            MH_EnableHook(pGetActive);
        }
        if (pGetRawInput) {
            MH_CreateHook(pGetRawInput, reinterpret_cast<LPVOID>(&HookedGetRawInputData), reinterpret_cast<void**>(&OriginalGetRawInputData));
            MH_EnableHook(pGetRawInput);
        }
        if (pGetCursor) {
            MH_CreateHook(pGetCursor, reinterpret_cast<LPVOID>(&HookedGetCursorPos), reinterpret_cast<void**>(&OriginalGetCursorPos));
            MH_EnableHook(pGetCursor);
        }
        if (pSetCursor) {
            MH_CreateHook(pSetCursor, reinterpret_cast<LPVOID>(&HookedSetCursor), reinterpret_cast<void**>(&OriginalSetCursor));
            MH_EnableHook(pSetCursor);
        }
    }

    LOG_INFO("InputHook: Input path: %s", m_usesRawInput ? "Raw Input" : "SendInput");

    StartBackgroundCapture();

    m_initialized = true;
    return true;
}

void InputHook::Shutdown() {
    StopBackgroundCapture();
    m_initialized = false;
}

void InputHook::UpdateEmulatedState(const XINPUT_STATE& state) {
    m_emulatedState = state;
}

DWORD WINAPI InputHook::HookedXInputGetState(DWORD dwUserIndex, XINPUT_STATE* pState) {
    InputHook& self = GetInstance();
    
    if (self.OriginalXInputGetState) {
        DWORD res = self.OriginalXInputGetState(dwUserIndex, pState);
        
        if (dwUserIndex == 0 && pState) {
            if (res == ERROR_SUCCESS) {
                // Merge physical state with emulated VR state
                pState->Gamepad.wButtons |= self.m_emulatedState.Gamepad.wButtons;
                
                // Merge right stick (Head tracking + Physical stick)
                int newRX = (int)pState->Gamepad.sThumbRX + (int)self.m_emulatedState.Gamepad.sThumbRX;
                int newRY = (int)pState->Gamepad.sThumbRY + (int)self.m_emulatedState.Gamepad.sThumbRY;
                
                // Clamp right stick
                pState->Gamepad.sThumbRX = (SHORT)(newRX > 32767 ? 32767 : (newRX < -32768 ? -32768 : newRX));
                pState->Gamepad.sThumbRY = (SHORT)(newRY > 32767 ? 32767 : (newRY < -32768 ? -32768 : newRY));
                // Merge left stick (VR thumbstick + Physical stick)
                int newLX = (int)pState->Gamepad.sThumbLX + (int)self.m_emulatedState.Gamepad.sThumbLX;
                int newLY = (int)pState->Gamepad.sThumbLY + (int)self.m_emulatedState.Gamepad.sThumbLY;
                
                pState->Gamepad.sThumbLX = (SHORT)(newLX > 32767 ? 32767 : (newLX < -32768 ? -32768 : newLX));
                pState->Gamepad.sThumbLY = (SHORT)(newLY > 32767 ? 32767 : (newLY < -32768 ? -32768 : newLY));
                
                // Merge triggers
                int newLT = (int)pState->Gamepad.bLeftTrigger + (int)self.m_emulatedState.Gamepad.bLeftTrigger;
                int newRT = (int)pState->Gamepad.bRightTrigger + (int)self.m_emulatedState.Gamepad.bRightTrigger;
                pState->Gamepad.bLeftTrigger = (BYTE)(newLT > 255 ? 255 : newLT);
                pState->Gamepad.bRightTrigger = (BYTE)(newRT > 255 ? 255 : newRT);
                
                pState->dwPacketNumber += self.m_emulatedState.dwPacketNumber;
                return ERROR_SUCCESS;
            } else {
                // No physical controller connected, fallback to purely emulated VR controllers
                // Only hijack if the user actually touches the VR controller (prevents locking out KBM for Null driver users)
                static bool hasUsedVRController = false;
                if (!hasUsedVRController && 
                    (self.m_emulatedState.Gamepad.wButtons != 0 || 
                     self.m_emulatedState.Gamepad.bLeftTrigger > 0 || 
                     self.m_emulatedState.Gamepad.bRightTrigger > 0 ||
                     self.m_emulatedState.Gamepad.sThumbLX != 0 ||
                     self.m_emulatedState.Gamepad.sThumbLY != 0 ||
                     self.m_emulatedState.Gamepad.sThumbRX != 0 ||
                     self.m_emulatedState.Gamepad.sThumbRY != 0)) {
                    hasUsedVRController = true;
                }
                
                if (hasUsedVRController && (self.m_vrControllersActive || self.m_captureActive)) {
                    *pState = self.m_emulatedState;
                    return ERROR_SUCCESS;
                }
                return res;
            }
        }
        return res;
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

    // Standard SendInput path. This generates hardware-level mouse events that the OS 
    // will naturally translate into both standard WM_MOUSEMOVE and raw WM_INPUT messages.
    // Forging WM_INPUT messages manually via PostMessage with a stack pointer causes Access Violations.
    INPUT inp         = {};
    inp.type          = INPUT_MOUSE;
    inp.mi.dwFlags    = MOUSEEVENTF_MOVE;
    inp.mi.dx         = dx;
    inp.mi.dy         = dy;
    SendInput(1, &inp, sizeof(INPUT));
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId() && GetWindow(hwnd, GW_OWNER) == 0 && IsWindowVisible(hwnd)) {
        char title[256];
        GetWindowTextA(hwnd, title, sizeof(title));
        if (strlen(title) > 0) {
            *(HWND*)lParam = hwnd;
            return FALSE; // found it
        }
    }
    return TRUE;
}

void InputHook::FindTargetWindow() {
    EnumWindows(EnumWindowsProc, (LPARAM)&m_targetHwnd);
    if (m_targetHwnd) {
        LOG_INFO("InputHook: Found target game window HWND: %p", m_targetHwnd);
        // Initially enable background capture
        m_captureActive = true;
        ToggleRawInputSink(true);
    }
}

void InputHook::ToggleRawInputSink(bool enable) {
    if (!m_targetHwnd) return;
    
    RAWINPUTDEVICE rid = {};
    rid.usUsagePage = 0x01; // Generic Desktop Controls
    rid.usUsage = 0x02;     // Mouse
    
    if (enable) {
        rid.dwFlags = RIDEV_INPUTSINK;
        rid.hwndTarget = m_targetHwnd;
    } else {
        rid.dwFlags = RIDEV_REMOVE;
        rid.hwndTarget = nullptr;
    }
    
    if (RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
        LOG_INFO("InputHook: RawInput Background Sink %s", enable ? "ENABLED" : "DISABLED");
    } else {
        LOG_ERROR("InputHook: Failed to toggle RawInput Sink.");
    }
}

void InputHook::StartBackgroundCapture() {
    if (m_captureRunning) return;
    m_captureRunning = true;
    m_captureThread = std::thread(&InputHook::CaptureThreadLoop, this);
}

void InputHook::StopBackgroundCapture() {
    m_captureRunning = false;
    if (m_captureThread.joinable()) {
        // Post a dummy message to wake up GetMessage
        PostThreadMessage(GetThreadId(m_captureThread.native_handle()), WM_QUIT, 0, 0);
        m_captureThread.join();
    }
}

void InputHook::CaptureThreadLoop() {
    LOG_INFO("InputHook: Background Capture Thread Started.");
    FindTargetWindow();

    m_keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(nullptr), 0);
    m_mouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, GetModuleHandle(nullptr), 0);

    MSG msg;
    while (m_captureRunning && GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_QUIT) break;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (m_keyboardHook) UnhookWindowsHookEx(m_keyboardHook);
    if (m_mouseHook) UnhookWindowsHookEx(m_mouseHook);
    m_keyboardHook = nullptr;
    m_mouseHook = nullptr;
    LOG_INFO("InputHook: Background Capture Thread Stopped.");
}

LRESULT CALLBACK InputHook::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    InputHook& self = GetInstance();
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* kbd = (KBDLLHOOKSTRUCT*)lParam;
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            // Use INSERT instead of F12 since F12 is Steam screenshot
            if (kbd->vkCode == VK_INSERT) {
                self.m_captureActive = !self.m_captureActive;
                LOG_INFO("InputHook: Background Capture Toggled: %s", self.m_captureActive ? "ON" : "OFF");
                self.ToggleRawInputSink(self.m_captureActive);
                return CallNextHookEx(nullptr, nCode, wParam, lParam);
            }
        }
        
        if (self.m_captureActive && self.m_targetHwnd) {
            // Don't swallow important system keys
            if (kbd->vkCode == VK_TAB && (GetAsyncKeyState(VK_MENU) & 0x8000)) {
                return CallNextHookEx(nullptr, nCode, wParam, lParam);
            }
            if (kbd->vkCode == VK_LWIN || kbd->vkCode == VK_RWIN || kbd->vkCode == VK_ESCAPE) {
                return CallNextHookEx(nullptr, nCode, wParam, lParam);
            }

            LPARAM postLParam = 1; // Repeat count
            postLParam |= (kbd->scanCode << 16);
            if (kbd->flags & LLKHF_EXTENDED) postLParam |= (1 << 24);
            if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
                postLParam |= (1 << 30); // Previous key state
                postLParam |= (1 << 31); // Transition state
            }
            PostMessageA(self.m_targetHwnd, (UINT)wParam, kbd->vkCode, postLParam);
            return 1; // Swallow input
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT CALLBACK InputHook::LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    InputHook& self = GetInstance();
    if (nCode == HC_ACTION && self.m_targetHwnd && self.m_captureActive) {
        MSLLHOOKSTRUCT* ms = (MSLLHOOKSTRUCT*)lParam;
        
        // Ignore events generated by SetCursorPos
        if (ms->flags & LLMHF_INJECTED) {
            return 1; // Still swallow it so it doesn't affect other apps
        }

        POINT pt = ms->pt;
        ScreenToClient(self.m_targetHwnd, &pt);
        LPARAM postLParam = MAKELPARAM(pt.x, pt.y);
            // --- GAMEPLAY MODE (INFINITE MOUSELOOK + FAKE VR CURSOR) ---
            if (wParam == WM_MOUSEMOVE) {
                static POINT lastScreenPt = ms->pt;
                int dx = ms->pt.x - lastScreenPt.x;
                int dy = ms->pt.y - lastScreenPt.y;
                
                self.m_mouseDeltaX += dx;
                self.m_mouseDeltaY += dy;
                
                self.m_virtualCursorX += dx;
                self.m_virtualCursorY += dy;
                
                RECT rc;
                if (GetClientRect(self.m_targetHwnd, &rc)) {
                    if (self.m_virtualCursorX < 0) self.m_virtualCursorX = 0;
                    if (self.m_virtualCursorY < 0) self.m_virtualCursorY = 0;
                    if (self.m_virtualCursorX > rc.right) self.m_virtualCursorX = rc.right;
                    if (self.m_virtualCursorY > rc.bottom) self.m_virtualCursorY = rc.bottom;
                }
                
                int screenWidth = GetSystemMetrics(SM_CXSCREEN);
                int screenHeight = GetSystemMetrics(SM_CYSCREEN);
                int centerX = screenWidth / 2;
                int centerY = screenHeight / 2;
                
                // Recenter the OS cursor to prevent hitting screen edges
                SetCursorPos(centerX, centerY);
                lastScreenPt.x = centerX;
                lastScreenPt.y = centerY;
                
                // Inject RawInput for 3D Camera Rotation
                PostMessageA(self.m_targetHwnd, WM_INPUT, RIM_INPUT, (LPARAM)0xDEADBEEF);
            }
            
            // Inject UI messages so the fake VR cursor can click things in menus
            LPARAM virtualLParam = MAKELPARAM(self.m_virtualCursorX, self.m_virtualCursorY);
            if (wParam == WM_LBUTTONDOWN) {
                self.m_mouseButtonFlags |= RI_MOUSE_LEFT_BUTTON_DOWN;
                PostMessageA(self.m_targetHwnd, WM_LBUTTONDOWN, MK_LBUTTON, virtualLParam);
            } else if (wParam == WM_LBUTTONUP) {
                self.m_mouseButtonFlags |= RI_MOUSE_LEFT_BUTTON_UP;
                PostMessageA(self.m_targetHwnd, WM_LBUTTONUP, 0, virtualLParam);
            } else if (wParam == WM_RBUTTONDOWN) {
                self.m_mouseButtonFlags |= RI_MOUSE_RIGHT_BUTTON_DOWN;
                PostMessageA(self.m_targetHwnd, WM_RBUTTONDOWN, MK_RBUTTON, virtualLParam);
            } else if (wParam == WM_RBUTTONUP) {
                self.m_mouseButtonFlags |= RI_MOUSE_RIGHT_BUTTON_UP;
                PostMessageA(self.m_targetHwnd, WM_RBUTTONUP, 0, virtualLParam);
            } else if (wParam == WM_MOUSEWHEEL) {
                self.m_mouseWheel += (short)HIWORD(ms->mouseData);
                PostMessageA(self.m_targetHwnd, WM_MOUSEWHEEL, MAKEWPARAM(0, ms->mouseData >> 16), virtualLParam);
            }
            
            // Inject RawInput for 3D Camera Rotation AND Clicks
            PostMessageA(self.m_targetHwnd, WM_INPUT, RIM_INPUT, (LPARAM)0xDEADBEEF);
            
            // Swallow all physical input so the user doesn't click outside the game
            return 1; 
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

} // namespace vrinject
