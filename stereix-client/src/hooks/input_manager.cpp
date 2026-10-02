#include "hooks/input_manager.h"
#include "core/logger.h"
#include "hooks/input_hook.h"
#include "core/overlay_manager.h"
#include "imgui.h"
#include <vector>
#include <cmath>

namespace vrinject {

InputManager::InputManager() {}

InputManager::~InputManager() {
    if (m_actionSet != XR_NULL_HANDLE) {
        xrDestroyActionSet(m_actionSet);
        m_actionSet = XR_NULL_HANDLE;
    }
}

static XrResult CreateAction(XrActionSet set, XrActionType type, const char* name, const char* localizedName, XrAction* outAction) {
    XrActionCreateInfo info = {XR_TYPE_ACTION_CREATE_INFO};
    info.actionType = type;
    strcpy_s(info.actionName, name);
    strcpy_s(info.localizedActionName, localizedName);
    return xrCreateAction(set, &info, outAction);
}

bool InputManager::Initialize(XrInstance instance, XrSession session) {
    if (instance == XR_NULL_HANDLE || session == XR_NULL_HANDLE) {
        LOG_ERROR("InputManager: Invalid instance or session handle.");
        return false;
    }

    XrActionSetCreateInfo actionSetInfo = {XR_TYPE_ACTION_SET_CREATE_INFO};
    strcpy_s(actionSetInfo.actionSetName, "vrinject_gamepad");
    strcpy_s(actionSetInfo.localizedActionSetName, "VRInject Virtual Gamepad");
    
    XrResult res = xrCreateActionSet(instance, &actionSetInfo, &m_actionSet);
    if (XR_FAILED(res)) return false;
    
    // Create boolean actions
    CreateAction(m_actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "menu", "Menu", &m_actionMenu);
    CreateAction(m_actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "a_button", "A Button", &m_actionA);
    CreateAction(m_actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "b_button", "B Button", &m_actionB);
    CreateAction(m_actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "x_button", "X Button", &m_actionX);
    CreateAction(m_actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "y_button", "Y Button", &m_actionY);
    CreateAction(m_actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "thumbstick_click_left", "Left Stick Click", &m_actionThumbstickClickLeft);
    CreateAction(m_actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "thumbstick_click_right", "Right Stick Click", &m_actionThumbstickClickRight);
    
    // Create float actions (Triggers & Grips)
    CreateAction(m_actionSet, XR_ACTION_TYPE_FLOAT_INPUT, "trigger_left", "Left Trigger", &m_actionTriggerLeft);
    CreateAction(m_actionSet, XR_ACTION_TYPE_FLOAT_INPUT, "trigger_right", "Right Trigger", &m_actionTriggerRight);
    CreateAction(m_actionSet, XR_ACTION_TYPE_FLOAT_INPUT, "grip_left", "Left Grip", &m_actionGripLeft);
    CreateAction(m_actionSet, XR_ACTION_TYPE_FLOAT_INPUT, "grip_right", "Right Grip", &m_actionGripRight);
    
    // Create vector2 actions (Thumbsticks)
    CreateAction(m_actionSet, XR_ACTION_TYPE_VECTOR2F_INPUT, "thumbstick_left", "Left Thumbstick", &m_actionThumbstickLeft);
    CreateAction(m_actionSet, XR_ACTION_TYPE_VECTOR2F_INPUT, "thumbstick_right", "Right Thumbstick", &m_actionThumbstickRight);

    // P0.1: Create haptic output actions for two-way rumble
    CreateAction(m_actionSet, XR_ACTION_TYPE_VIBRATION_OUTPUT, "haptic_left", "Left Haptic", &m_actionHapticLeft);
    CreateAction(m_actionSet, XR_ACTION_TYPE_VIBRATION_OUTPUT, "haptic_right", "Right Haptic", &m_actionHapticRight);
    
    auto GetPath = [&](const char* str) -> XrPath {
        XrPath p;
        xrStringToPath(instance, str, &p);
        return p;
    };
    
    // --- Oculus Touch bindings ---
    std::vector<XrActionSuggestedBinding> oculusBindings = {
        {m_actionMenu, GetPath("/user/hand/left/input/menu/click")},
        {m_actionX, GetPath("/user/hand/left/input/x/click")},
        {m_actionY, GetPath("/user/hand/left/input/y/click")},
        {m_actionA, GetPath("/user/hand/right/input/a/click")},
        {m_actionB, GetPath("/user/hand/right/input/b/click")},
        {m_actionThumbstickClickLeft, GetPath("/user/hand/left/input/thumbstick/click")},
        {m_actionThumbstickClickRight, GetPath("/user/hand/right/input/thumbstick/click")},
        {m_actionTriggerLeft, GetPath("/user/hand/left/input/trigger/value")},
        {m_actionTriggerRight, GetPath("/user/hand/right/input/trigger/value")},
        {m_actionGripLeft, GetPath("/user/hand/left/input/squeeze/value")},
        {m_actionGripRight, GetPath("/user/hand/right/input/squeeze/value")},
        {m_actionThumbstickLeft, GetPath("/user/hand/left/input/thumbstick")},
        {m_actionThumbstickRight, GetPath("/user/hand/right/input/thumbstick")},
        // P0.1: Haptic outputs
        {m_actionHapticLeft, GetPath("/user/hand/left/output/haptic")},
        {m_actionHapticRight, GetPath("/user/hand/right/output/haptic")}
    };
    
    // --- Valve Index bindings ---
    std::vector<XrActionSuggestedBinding> indexBindings = {
        {m_actionMenu, GetPath("/user/hand/left/input/a/click")},
        {m_actionA, GetPath("/user/hand/right/input/a/click")},
        {m_actionB, GetPath("/user/hand/right/input/b/click")},
        {m_actionThumbstickClickLeft, GetPath("/user/hand/left/input/thumbstick/click")},
        {m_actionThumbstickClickRight, GetPath("/user/hand/right/input/thumbstick/click")},
        {m_actionTriggerLeft, GetPath("/user/hand/left/input/trigger/value")},
        {m_actionTriggerRight, GetPath("/user/hand/right/input/trigger/value")},
        {m_actionGripLeft, GetPath("/user/hand/left/input/squeeze/value")},
        {m_actionGripRight, GetPath("/user/hand/right/input/squeeze/value")},
        {m_actionThumbstickLeft, GetPath("/user/hand/left/input/thumbstick")},
        {m_actionThumbstickRight, GetPath("/user/hand/right/input/thumbstick")},
        // P0.1: Haptic outputs
        {m_actionHapticLeft, GetPath("/user/hand/left/output/haptic")},
        {m_actionHapticRight, GetPath("/user/hand/right/output/haptic")}
    };
    
    // --- Simple controller bindings (fallback) ---
    std::vector<XrActionSuggestedBinding> simpleBindings = {
        {m_actionMenu, GetPath("/user/hand/left/input/menu/click")},
        {m_actionA, GetPath("/user/hand/right/input/select/click")},
        {m_actionTriggerLeft, GetPath("/user/hand/left/input/select/click")},
        {m_actionTriggerRight, GetPath("/user/hand/right/input/select/click")},
        // P0.1: Haptic outputs
        {m_actionHapticLeft, GetPath("/user/hand/left/output/haptic")},
        {m_actionHapticRight, GetPath("/user/hand/right/output/haptic")}
    };

    XrInteractionProfileSuggestedBinding suggestedBindingsOculus = {XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    suggestedBindingsOculus.interactionProfile = GetPath("/interaction_profiles/oculus/touch_controller");
    suggestedBindingsOculus.suggestedBindings = oculusBindings.data();
    suggestedBindingsOculus.countSuggestedBindings = (uint32_t)oculusBindings.size();
    xrSuggestInteractionProfileBindings(instance, &suggestedBindingsOculus);

    XrInteractionProfileSuggestedBinding suggestedBindingsIndex = {XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    suggestedBindingsIndex.interactionProfile = GetPath("/interaction_profiles/valve/index_controller");
    suggestedBindingsIndex.suggestedBindings = indexBindings.data();
    suggestedBindingsIndex.countSuggestedBindings = (uint32_t)indexBindings.size();
    xrSuggestInteractionProfileBindings(instance, &suggestedBindingsIndex);
    
    XrInteractionProfileSuggestedBinding suggestedBindingsSimple = {XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    suggestedBindingsSimple.interactionProfile = GetPath("/interaction_profiles/khr/simple_controller");
    suggestedBindingsSimple.suggestedBindings = simpleBindings.data();
    suggestedBindingsSimple.countSuggestedBindings = (uint32_t)simpleBindings.size();
    xrSuggestInteractionProfileBindings(instance, &suggestedBindingsSimple);
    
    XrSessionActionSetsAttachInfo attachInfo = {XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attachInfo.countActionSets = 1;
    attachInfo.actionSets = &m_actionSet;
    xrAttachSessionActionSets(session, &attachInfo);
    
    LOG_INFO("InputManager: OpenXR input actions initialized (with haptics, D-Pad, Start/Back).");
    return true;
}

static bool GetBool(XrSession session, XrAction action) {
    XrActionStateGetInfo getInfo = {XR_TYPE_ACTION_STATE_GET_INFO};
    getInfo.action = action;
    XrActionStateBoolean state = {XR_TYPE_ACTION_STATE_BOOLEAN};
    if (XR_SUCCEEDED(xrGetActionStateBoolean(session, &getInfo, &state))) return state.currentState;
    return false;
}

static float GetFloat(XrSession session, XrAction action) {
    XrActionStateGetInfo getInfo = {XR_TYPE_ACTION_STATE_GET_INFO};
    getInfo.action = action;
    XrActionStateFloat state = {XR_TYPE_ACTION_STATE_FLOAT};
    if (XR_SUCCEEDED(xrGetActionStateFloat(session, &getInfo, &state))) return state.currentState;
    return 0.0f;
}

static XrVector2f GetVector2(XrSession session, XrAction action) {
    XrActionStateGetInfo getInfo = {XR_TYPE_ACTION_STATE_GET_INFO};
    getInfo.action = action;
    XrActionStateVector2f state = {XR_TYPE_ACTION_STATE_VECTOR2F};
    if (XR_SUCCEEDED(xrGetActionStateVector2f(session, &getInfo, &state))) return state.currentState;
    return {0.0f, 0.0f};
}

void InputManager::ApplyHaptics(XrSession session, float leftAmplitude, float rightAmplitude) {
    // Fire haptic pulses on the VR controllers corresponding to game vibration.
    // Duration is 100ms (game typically calls XInputSetState every frame).
    XrHapticVibration vibration = {XR_TYPE_HAPTIC_VIBRATION};
    vibration.duration = 100000000; // 100ms in nanoseconds
    vibration.frequency = XR_FREQUENCY_UNSPECIFIED;

    if (leftAmplitude > 0.01f) {
        vibration.amplitude = leftAmplitude;
        XrHapticActionInfo hapticInfo = {XR_TYPE_HAPTIC_ACTION_INFO};
        hapticInfo.action = m_actionHapticLeft;
        xrApplyHapticFeedback(session, &hapticInfo, reinterpret_cast<const XrHapticBaseHeader*>(&vibration));
    }

    if (rightAmplitude > 0.01f) {
        vibration.amplitude = rightAmplitude;
        XrHapticActionInfo hapticInfo = {XR_TYPE_HAPTIC_ACTION_INFO};
        hapticInfo.action = m_actionHapticRight;
        xrApplyHapticFeedback(session, &hapticInfo, reinterpret_cast<const XrHapticBaseHeader*>(&vibration));
    }
}

void InputManager::Update(XrSession session) {
    if (session == XR_NULL_HANDLE || m_actionSet == XR_NULL_HANDLE) return;

    XrActiveActionSet activeActionSet = {m_actionSet, XR_NULL_PATH};
    XrActionsSyncInfo syncInfo = {XR_TYPE_ACTIONS_SYNC_INFO};
    syncInfo.countActiveActionSets = 1;
    syncInfo.activeActionSets = &activeActionSet;
    
    if (XR_FAILED(xrSyncActions(session, &syncInfo))) return;
    
    XINPUT_STATE state = {};
    state.dwPacketNumber = 1;
    
    bool menuPressed = GetBool(session, m_actionMenu);
    bool thumbClickL = GetBool(session, m_actionThumbstickClickLeft);
    bool thumbClickR = GetBool(session, m_actionThumbstickClickRight);
    bool dualStickClick = thumbClickL && thumbClickR;
    bool toggleRequested = menuPressed || dualStickClick;

    static bool s_lastToggleRequested = false;
    if (toggleRequested && !s_lastToggleRequested) {
        OverlayManager::GetInstance().ToggleOverlay();
        LOG_INFO("OverlayManager: In-headset menu toggled via VR motion controller (%s)",
                 OverlayManager::GetInstance().IsOverlayVisible() ? "OPEN" : "CLOSED");
    }
    s_lastToggleRequested = toggleRequested;

    // --- P0.1: D-Pad Emulation ---
    // When left stick is clicked and held, left thumbstick axes map to D-Pad directions
    // instead of analog movement. This enables item cycling in Sekiro, Elden Ring, etc.
    XrVector2f thumbL = GetVector2(session, m_actionThumbstickLeft);
    XrVector2f thumbR = GetVector2(session, m_actionThumbstickRight);
    
    bool leftStickClickNow = thumbClickL && !dualStickClick;
    m_leftStickClickHeld = leftStickClickNow;

    if (m_leftStickClickHeld) {
        // D-Pad mode: cardinal directions from left thumbstick
        const float DPAD_THRESHOLD = 0.5f;
        if (thumbL.y >  DPAD_THRESHOLD) state.Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_UP;
        if (thumbL.y < -DPAD_THRESHOLD) state.Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_DOWN;
        if (thumbL.x < -DPAD_THRESHOLD) state.Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_LEFT;
        if (thumbL.x >  DPAD_THRESHOLD) state.Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_RIGHT;
        // Don't send analog stick movement while in D-Pad mode
        thumbL = {0.0f, 0.0f};
    }

    // --- Standard button mapping ---
    if (GetBool(session, m_actionA)) state.Gamepad.wButtons |= XINPUT_GAMEPAD_A;
    if (GetBool(session, m_actionB)) state.Gamepad.wButtons |= XINPUT_GAMEPAD_B;
    if (GetBool(session, m_actionX)) state.Gamepad.wButtons |= XINPUT_GAMEPAD_X;
    if (GetBool(session, m_actionY)) state.Gamepad.wButtons |= XINPUT_GAMEPAD_Y;
    if (leftStickClickNow && !m_leftStickClickHeld) state.Gamepad.wButtons |= XINPUT_GAMEPAD_LEFT_THUMB;
    if (thumbClickR && !dualStickClick) state.Gamepad.wButtons |= XINPUT_GAMEPAD_RIGHT_THUMB;

    // P0.1: Left Grip → Left Shoulder (LB), Right Grip → Right Shoulder (RB)
    float gripL = GetFloat(session, m_actionGripLeft);
    float gripR = GetFloat(session, m_actionGripRight);
    if (gripL > 0.5f) state.Gamepad.wButtons |= XINPUT_GAMEPAD_LEFT_SHOULDER;
    if (gripR > 0.5f) state.Gamepad.wButtons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;

    // P0.1: Start/Back mapping via grip modifier
    // Left Grip + A → Start,  Left Grip + B → Back
    if (gripL > 0.5f) {
        if (GetBool(session, m_actionA)) state.Gamepad.wButtons |= XINPUT_GAMEPAD_START;
        if (GetBool(session, m_actionB)) state.Gamepad.wButtons |= XINPUT_GAMEPAD_BACK;
    }
    
    state.Gamepad.bLeftTrigger = (BYTE)(GetFloat(session, m_actionTriggerLeft) * 255.0f);
    state.Gamepad.bRightTrigger = (BYTE)(GetFloat(session, m_actionTriggerRight) * 255.0f);
    
    state.Gamepad.sThumbLX = (SHORT)(thumbL.x * 32767.0f);
    state.Gamepad.sThumbLY = (SHORT)(thumbL.y * 32767.0f);
    state.Gamepad.sThumbRX = (SHORT)(thumbR.x * 32767.0f);
    state.Gamepad.sThumbRY = (SHORT)(thumbR.y * 32767.0f);
    
    // --- P0.1: Two-Way Rumble Haptics ---
    // Consume vibration values stored by HookedXInputSetState and apply them
    // as OpenXR haptic feedback on the VR motion controllers.
    float vibLeft = 0.0f, vibRight = 0.0f;
    if (InputHook::GetInstance().ConsumeVibration(vibLeft, vibRight)) {
        ApplyHaptics(session, vibLeft, vibRight);
    }

    // Determine if user is actively using VR controllers
    bool isActive = state.Gamepad.wButtons != 0 || state.Gamepad.bLeftTrigger > 0 || state.Gamepad.bRightTrigger > 0 ||
                    abs(state.Gamepad.sThumbLX) > 2000 || abs(state.Gamepad.sThumbLY) > 2000 ||
                    abs(state.Gamepad.sThumbRX) > 2000 || abs(state.Gamepad.sThumbRY) > 2000;
                    
    InputHook::GetInstance().SetVRControllersActive(isActive);
    InputHook::GetInstance().UpdateEmulatedState(state);
}

} // namespace vrinject
