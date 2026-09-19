#include "hooks/input_hook.h"
#include "core/logger.h"
#include "core/config_manager.h"
#include "core/subsystem_context.h"
#include "openxr/openxr_runtime_manager.h"
#include "MinHook.h"
#include <vector>
#include "core/overlay_manager.h"

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

WNDPROC g_OriginalWndProc = nullptr;
LRESULT CALLBACK HookedWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    __try {
        if (OverlayManager::GetInstance().HandleWndProc(hwnd, msg, wParam, lParam)) {
            return true; // ImGui captured the input
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    // Focus preservation: Keep the game active and listening to input even when running with VR/null driver
    switch (msg) {
        case WM_ACTIVATEAPP:
            wParam = TRUE; // Force application active
            break;
        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE) {
                wParam = MAKEWPARAM(WA_ACTIVE, HIWORD(wParam)); // Maintain active state
            }
            break;
        case WM_NCACTIVATE:
            wParam = TRUE; // Keep title bar styled active
            break;
        case WM_KILLFOCUS:
            // Suppress loss of focus so game does not cancel mouse capture or keyboard input
            return 0;
        case WM_MOUSEMOVE: {
            static int s_prevX = -1, s_prevY = -1;
            int curX = static_cast<short>(LOWORD(lParam));
            int curY = static_cast<short>(HIWORD(lParam));
            if (s_prevX != -1) {
                int dx = curX - s_prevX;
                int dy = curY - s_prevY;
                if (std::abs(dx) > 0 && std::abs(dx) < 300 && std::abs(dy) < 300) {
                    InputHook::GetInstance().RecordPhysicalMouseDelta(dx, dy);
                }
            }
            s_prevX = curX;
            s_prevY = curY;
            break;
        }
    }

    if (g_OriginalWndProc) {
        return g_OriginalWndProc(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

HWND WINAPI HookedGetForegroundWindow() {
    HWND target = InputHook::GetInstance().GetTargetHwnd();
    if (target) return target;
    if (OriginalGetForegroundWindow) return OriginalGetForegroundWindow();
    return nullptr;
}

HWND WINAPI HookedGetActiveWindow() {
    HWND target = InputHook::GetInstance().GetTargetHwnd();
    if (target) return target;
    if (OriginalGetActiveWindow) return OriginalGetActiveWindow();
    return nullptr;
}

// FIX #13: Replace the hardcoded 0xDEADBEEF magic handle with a randomized
// per-session secret. External code cannot predict this value and call us.
// Using function-local static for thread-safe initialization (C++11).
static UINT_PTR GetRawInputMagicHandle() {
    static UINT_PTR g_rawInputMagicHandle = []() {
        // Mix a high-entropy value: base address of the DLL XOR a stack address.
        const UINT_PTR MAGIC_HANDLE_SENTINEL = 0xDEADB00F;
        const UINT_PTR FALLBACK_MAGIC_HANDLE = 0xCAFE1234;
        UINT_PTR handle = reinterpret_cast<UINT_PTR>(&handle) ^
                          reinterpret_cast<UINT_PTR>(GetModuleHandleA(nullptr)) ^
                          MAGIC_HANDLE_SENTINEL; // Non-zero sentinel for safety
        if (handle == 0) handle = FALLBACK_MAGIC_HANDLE; // fallback if still 0
        return handle;
    }();
    return g_rawInputMagicHandle;
}

UINT WINAPI HookedGetRawInputData(HRAWINPUT hRawInput, UINT uiCommand, LPVOID pData, PUINT pcbSize, UINT cbSizeHeader) {
    // FIX #13: Only inject synthetic data for the secret per-session handle,
    // and only when the VR capture is actually active.
    if ((uintptr_t)hRawInput == GetRawInputMagicHandle() &&
        InputHook::GetInstance().IsCaptureActive()) {
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
    if (OriginalGetRawInputData) {
        UINT res = OriginalGetRawInputData(hRawInput, uiCommand, pData, pcbSize, cbSizeHeader);
        if (res != (UINT)-1 && pData && uiCommand == RID_INPUT && pcbSize && *pcbSize >= sizeof(RAWINPUT)) {
            RAWINPUT* ri = reinterpret_cast<RAWINPUT*>(pData);
            if (ri->header.dwType == RIM_TYPEMOUSE && (ri->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0) {
                InputHook::GetInstance().RecordPhysicalMouseDelta(ri->data.mouse.lLastX, ri->data.mouse.lLastY);
            }
        }
        return res;
    }
    return (UINT)-1;
}

BOOL WINAPI HookedGetCursorPos(LPPOINT lpPoint) {
    if (OriginalGetCursorPos) return OriginalGetCursorPos(lpPoint);
    return GetCursorPos(lpPoint);
}

HCURSOR WINAPI HookedSetCursor(HCURSOR hCursor) {
    if (OriginalSetCursor) return OriginalSetCursor(hCursor);
    return SetCursor(hCursor);
}

bool InputHook::Initialize() {
    if (m_initialized) return true;

    if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED) {
        LOG_ERROR("InputHook: Failed to initialize MinHook.");
        return false;
    }

    // 1. Try hooking XInput 1.4, 1.3, and 9.1.0 as different games use different versions.
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

    // 2. Hook User32 APIs for focus spoofing and seamless mouse/keyboard input
    HMODULE hUser32 = GetModuleHandleA("user32.dll");
    if (!hUser32) hUser32 = LoadLibraryA("user32.dll");
    if (hUser32) {
        void* pGetForegroundWindow = (void*)GetProcAddress(hUser32, "GetForegroundWindow");
        void* pGetActiveWindow = (void*)GetProcAddress(hUser32, "GetActiveWindow");
        void* pGetCursorPos = (void*)GetProcAddress(hUser32, "GetCursorPos");
        void* pSetCursor = (void*)GetProcAddress(hUser32, "SetCursor");
        void* pGetRawInputData = (void*)GetProcAddress(hUser32, "GetRawInputData");

        if (pGetForegroundWindow) {
            MH_CreateHook(pGetForegroundWindow, reinterpret_cast<LPVOID>(&HookedGetForegroundWindow), reinterpret_cast<void**>(&OriginalGetForegroundWindow));
            MH_EnableHook(pGetForegroundWindow);
            LOG_DEBUG("InputHook: Hooked GetForegroundWindow.");
        }
        if (pGetActiveWindow) {
            MH_CreateHook(pGetActiveWindow, reinterpret_cast<LPVOID>(&HookedGetActiveWindow), reinterpret_cast<void**>(&OriginalGetActiveWindow));
            MH_EnableHook(pGetActiveWindow);
            LOG_DEBUG("InputHook: Hooked GetActiveWindow.");
        }
        if (pGetCursorPos) {
            MH_CreateHook(pGetCursorPos, reinterpret_cast<LPVOID>(&HookedGetCursorPos), reinterpret_cast<void**>(&OriginalGetCursorPos));
            MH_EnableHook(pGetCursorPos);
            LOG_DEBUG("InputHook: Hooked GetCursorPos.");
        }
        if (pSetCursor) {
            MH_CreateHook(pSetCursor, reinterpret_cast<LPVOID>(&HookedSetCursor), reinterpret_cast<void**>(&OriginalSetCursor));
            MH_EnableHook(pSetCursor);
            LOG_DEBUG("InputHook: Hooked SetCursor.");
        }
        if (pGetRawInputData) {
            MH_CreateHook(pGetRawInputData, reinterpret_cast<LPVOID>(&HookedGetRawInputData), reinterpret_cast<void**>(&OriginalGetRawInputData));
            MH_EnableHook(pGetRawInputData);
            LOG_DEBUG("InputHook: Hooked GetRawInputData.");
        }
    }

    // 3. Start background capture thread for keyboard and mouse
    StartBackgroundCapture();

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

    LOG_INFO("[OK] Input System: VR Controllers & Gamepad Hooked (%s)", m_usesRawInput ? "Raw Input" : "SendInput");

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
                // Option A: Record physical gamepad look input
                const SHORT DEADZONE = 3500;
                SHORT rx = pState->Gamepad.sThumbRX;
                SHORT ry = pState->Gamepad.sThumbRY;
                if (std::abs(rx) > DEADZONE || std::abs(ry) > DEADZONE) {
                    self.RecordThumbstickDelta(static_cast<float>(rx) / 32768.0f, static_cast<float>(ry) / 32768.0f);
                }

                // Physical controller is connected. If VR controllers are actively used, merge them:
                if (self.m_vrControllersActive) {
                    pState->Gamepad.wButtons |= self.m_emulatedState.Gamepad.wButtons;
                    
                    int newRX = (int)pState->Gamepad.sThumbRX + (int)self.m_emulatedState.Gamepad.sThumbRX;
                    int newRY = (int)pState->Gamepad.sThumbRY + (int)self.m_emulatedState.Gamepad.sThumbRY;
                    pState->Gamepad.sThumbRX = (SHORT)(newRX > 32767 ? 32767 : (newRX < -32768 ? -32768 : newRX));
                    pState->Gamepad.sThumbRY = (SHORT)(newRY > 32767 ? 32767 : (newRY < -32768 ? -32768 : newRY));
                    int newLX = (int)pState->Gamepad.sThumbLX + (int)self.m_emulatedState.Gamepad.sThumbLX;
                    int newLY = (int)pState->Gamepad.sThumbLY + (int)self.m_emulatedState.Gamepad.sThumbLY;
                    pState->Gamepad.sThumbLX = (SHORT)(newLX > 32767 ? 32767 : (newLX < -32768 ? -32768 : newLX));
                    pState->Gamepad.sThumbLY = (SHORT)(newLY > 32767 ? 32767 : (newLY < -32768 ? -32768 : newLY));
                    
                    int newLT = (int)pState->Gamepad.bLeftTrigger + (int)self.m_emulatedState.Gamepad.bLeftTrigger;
                    int newRT = (int)pState->Gamepad.bRightTrigger + (int)self.m_emulatedState.Gamepad.bRightTrigger;
                    pState->Gamepad.bLeftTrigger = (BYTE)(newLT > 255 ? 255 : newLT);
                    pState->Gamepad.bRightTrigger = (BYTE)(newRT > 255 ? 255 : newRT);
                    
                    pState->dwPacketNumber += self.m_emulatedState.dwPacketNumber;
                }
                return ERROR_SUCCESS;
            } else {
                if (self.m_vrControllersActive) {
                    *pState = self.m_emulatedState;
                    const SHORT DEADZONE = 3500;
                    SHORT rx = pState->Gamepad.sThumbRX;
                    SHORT ry = pState->Gamepad.sThumbRY;
                    if (std::abs(rx) > DEADZONE || std::abs(ry) > DEADZONE) {
                        self.RecordThumbstickDelta(static_cast<float>(rx) / 32768.0f, static_cast<float>(ry) / 32768.0f);
                    }
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
        // if (self.m_openxrManager) {
        //     float left = (float)pVibration->wLeftMotorSpeed / 65535.0f;
        //     float right = (float)pVibration->wRightMotorSpeed / 65535.0f;
        //     self.m_openxrManager->ApplyHapticFeedback(left, right);
        // }
        return ERROR_SUCCESS;
    }

    if (self.OriginalXInputSetState) {
        return self.OriginalXInputSetState(dwUserIndex, pVibration);
    }
    return ERROR_DEVICE_NOT_CONNECTED;
}

void InputHook::InjectAimDelta(float pitchDeg, float yawDeg) {
    float sens = SubsystemContext::Get().GetConfig()->GetConfig().motionAimSensitivity;

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

void InputHook::SetTargetHwnd(HWND hwnd) {
    if (!hwnd) return;
    if (m_targetHwnd == hwnd && g_OriginalWndProc != nullptr) return;
    m_targetHwnd = hwnd;
    LOG_DEBUG("InputHook: Target game window HWND set to: %p", m_targetHwnd);
    
    WNDPROC currentWndProc = (WNDPROC)GetWindowLongPtr(m_targetHwnd, GWLP_WNDPROC);
    if (currentWndProc && currentWndProc != (WNDPROC)&HookedWndProc && !g_OriginalWndProc) {
        MH_CreateHook((LPVOID)currentWndProc, (LPVOID)&HookedWndProc, (reinterpret_cast<LPVOID*>(&g_OriginalWndProc)));
        MH_EnableHook((LPVOID)currentWndProc);
        LOG_DEBUG("InputHook: Successfully hooked WndProc via MinHook.");
    }
}

void InputHook::FindTargetWindow() {
    for (int retry = 0; retry < 10 && !m_targetHwnd && m_captureRunning; ++retry) {
        EnumWindows(EnumWindowsProc, (LPARAM)&m_targetHwnd);
        if (m_targetHwnd) break;
        Sleep(50);
    }
    if (m_targetHwnd) {
        LOG_DEBUG("InputHook: Found target game window HWND: %p", m_targetHwnd);
        SetTargetHwnd(m_targetHwnd);
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
        LOG_DEBUG("InputHook: RawInput Background Sink %s", enable ? "ENABLED" : "DISABLED");
    } else {
        LOG_ERROR("InputHook: Failed to toggle RawInput Sink.");
    }
}

void InputHook::StartBackgroundCapture() {
    if (m_captureRunning) return;
    m_captureRunning = true;
    m_captureThreadReady = false;
    m_captureThread = std::thread(&InputHook::CaptureThreadLoop, this);
}

void InputHook::StopBackgroundCapture() {
    if (m_captureThread.joinable()) {
        m_captureRunning = false;

        // Wait until the thread has created its message queue
        {
            std::unique_lock<std::mutex> lock(m_captureMutex);
            m_captureCv.wait(lock, [this] { return m_captureThreadReady.load(); });
        }

        DWORD threadId = GetThreadId(m_captureThread.native_handle());
        BOOL posted = FALSE;
        for (int i = 0; i < 20 && !posted; ++i) {
            posted = PostThreadMessage(threadId, WM_QUIT, 0, 0);
            if (!posted) {
                Sleep(10);
            }
        }
        
        m_captureThread.join();
        m_captureThreadReady = false;
    }
}

void InputHook::CaptureThreadLoop() {
    LOG_DEBUG("InputHook: Background Capture Thread Started.");
    FindTargetWindow();

    HMODULE hDll = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)&InputHook::LowLevelKeyboardProc,
        &hDll
    );
    if (!hDll) hDll = GetModuleHandle(nullptr);

    m_keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hDll, 0);
    if (!m_keyboardHook) {
        LOG_WARN("InputHook: SetWindowsHookEx(WH_KEYBOARD_LL) failed (code %lu)", GetLastError());
    }
    m_mouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, hDll, 0);
    if (!m_mouseHook) {
        LOG_WARN("InputHook: SetWindowsHookEx(WH_MOUSE_LL) failed (code %lu)", GetLastError());
    }

    // Force creation of the message queue for this thread
    MSG msg;
    PeekMessage(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);

    // Signal that the thread and its message queue are fully ready
    {
        std::lock_guard<std::mutex> lock(m_captureMutex);
        m_captureThreadReady = true;
    }
    m_captureCv.notify_one();

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
    if (nCode == HC_ACTION && self.m_targetHwnd) {
        KBDLLHOOKSTRUCT* kbd = (KBDLLHOOKSTRUCT*)lParam;

        HWND fgHwnd = OriginalGetForegroundWindow ? OriginalGetForegroundWindow() : GetForegroundWindow();
        if (fgHwnd == self.m_targetHwnd) {
            // Target window has real OS focus; let Windows deliver naturally
            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        }

        // Forward to target game window if focus is currently on SteamVR mirror or another window
        if (kbd->vkCode == VK_TAB && (GetAsyncKeyState(VK_MENU) & 0x8000)) {
            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        }
        if (kbd->vkCode == VK_LWIN || kbd->vkCode == VK_RWIN) {
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
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT CALLBACK InputHook::LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    InputHook& self = GetInstance();
    if (nCode == HC_ACTION && self.m_targetHwnd) {
        MSLLHOOKSTRUCT* ms = (MSLLHOOKSTRUCT*)lParam;

        if (ms->flags & LLMHF_INJECTED) {
            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        }

        HWND fgHwnd = OriginalGetForegroundWindow ? OriginalGetForegroundWindow() : GetForegroundWindow();
        if (fgHwnd == self.m_targetHwnd) {
            // Target window has real OS focus; let Windows deliver naturally
            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        }

        // Forward mouse clicks and movement to game window when SteamVR mirror or another window is active
        POINT pt = ms->pt;
        ScreenToClient(self.m_targetHwnd, &pt);
        LPARAM clientLParam = MAKELPARAM(pt.x, pt.y);

        if (wParam == WM_LBUTTONDOWN) {
            PostMessageA(self.m_targetHwnd, WM_LBUTTONDOWN, MK_LBUTTON, clientLParam);
        } else if (wParam == WM_LBUTTONUP) {
            PostMessageA(self.m_targetHwnd, WM_LBUTTONUP, 0, clientLParam);
        } else if (wParam == WM_RBUTTONDOWN) {
            PostMessageA(self.m_targetHwnd, WM_RBUTTONDOWN, MK_RBUTTON, clientLParam);
        } else if (wParam == WM_RBUTTONUP) {
            PostMessageA(self.m_targetHwnd, WM_RBUTTONUP, 0, clientLParam);
        } else if (wParam == WM_MBUTTONDOWN) {
            PostMessageA(self.m_targetHwnd, WM_MBUTTONDOWN, MK_MBUTTON, clientLParam);
        } else if (wParam == WM_MBUTTONUP) {
            PostMessageA(self.m_targetHwnd, WM_MBUTTONUP, 0, clientLParam);
        } else if (wParam == WM_MOUSEWHEEL) {
            PostMessageA(self.m_targetHwnd, WM_MOUSEWHEEL, MAKEWPARAM(0, HIWORD(ms->mouseData)), clientLParam);
        } else if (wParam == WM_MOUSEMOVE) {
            PostMessageA(self.m_targetHwnd, WM_MOUSEMOVE, 0, clientLParam);
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

void InputHook::RecordPhysicalMouseDelta(int dx, int dy) {
    m_observedPhysicalMouseDeltaX.fetch_add(dx, std::memory_order_relaxed);
    m_observedPhysicalMouseDeltaY.fetch_add(dy, std::memory_order_relaxed);
}

void InputHook::RecordThumbstickDelta(float rx, float ry) {
    m_observedThumbDeltaX.store(static_cast<int>(rx * 1000.0f), std::memory_order_relaxed);
    m_observedThumbDeltaY.store(static_cast<int>(ry * 1000.0f), std::memory_order_relaxed);
}

void InputHook::ConsumeAccumulatedInputDeltas(float& outMouseDelta, float& outStickDelta) {
    int mdx = m_observedPhysicalMouseDeltaX.exchange(0, std::memory_order_relaxed);
    int mdy = m_observedPhysicalMouseDeltaY.exchange(0, std::memory_order_relaxed);
    int stx = m_observedThumbDeltaX.exchange(0, std::memory_order_relaxed);
    int sty = m_observedThumbDeltaY.exchange(0, std::memory_order_relaxed);

    outMouseDelta = std::sqrt(static_cast<float>(mdx * mdx + mdy * mdy));
    outStickDelta = std::sqrt(static_cast<float>(stx * stx + sty * sty)) / 1000.0f;
}

} // namespace vrinject
