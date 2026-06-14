#include "input_manager.h"
#include "../core/logger.h"
#include "input_hook.h"
#include <vector>

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
    
    auto GetPath = [&](const char* str) -> XrPath {
        XrPath p;
        xrStringToPath(instance, str, &p);
        return p;
    };
    
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
        {m_actionThumbstickRight, GetPath("/user/hand/right/input/thumbstick")}
    };
    
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
        {m_actionThumbstickRight, GetPath("/user/hand/right/input/thumbstick")}
    };
    
    std::vector<XrActionSuggestedBinding> simpleBindings = {
        {m_actionMenu, GetPath("/user/hand/left/input/menu/click")},
        {m_actionA, GetPath("/user/hand/right/input/select/click")},
        {m_actionTriggerLeft, GetPath("/user/hand/left/input/select/click")},
        {m_actionTriggerRight, GetPath("/user/hand/right/input/select/click")}
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
    
    LOG_INFO("InputManager: OpenXR input actions initialized successfully.");
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

void InputManager::Update(XrSession session) {
    if (session == XR_NULL_HANDLE || m_actionSet == XR_NULL_HANDLE) return;

    XrActiveActionSet activeActionSet = {m_actionSet, XR_NULL_PATH};
    XrActionsSyncInfo syncInfo = {XR_TYPE_ACTIONS_SYNC_INFO};
    syncInfo.countActiveActionSets = 1;
    syncInfo.activeActionSets = &activeActionSet;
    
    if (XR_FAILED(xrSyncActions(session, &syncInfo))) return;
    
    XINPUT_STATE state = {};
    state.dwPacketNumber = 1;
    
    if (GetBool(session, m_actionA)) state.Gamepad.wButtons |= XINPUT_GAMEPAD_A;
    if (GetBool(session, m_actionB)) state.Gamepad.wButtons |= XINPUT_GAMEPAD_B;
    if (GetBool(session, m_actionX)) state.Gamepad.wButtons |= XINPUT_GAMEPAD_X;
    if (GetBool(session, m_actionY)) state.Gamepad.wButtons |= XINPUT_GAMEPAD_Y;
    if (GetBool(session, m_actionMenu)) state.Gamepad.wButtons |= XINPUT_GAMEPAD_START;
    if (GetBool(session, m_actionThumbstickClickLeft)) state.Gamepad.wButtons |= XINPUT_GAMEPAD_LEFT_THUMB;
    if (GetBool(session, m_actionThumbstickClickRight)) state.Gamepad.wButtons |= XINPUT_GAMEPAD_RIGHT_THUMB;
    if (GetFloat(session, m_actionGripLeft) > 0.5f) state.Gamepad.wButtons |= XINPUT_GAMEPAD_LEFT_SHOULDER;
    if (GetFloat(session, m_actionGripRight) > 0.5f) state.Gamepad.wButtons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;
    
    state.Gamepad.bLeftTrigger = (BYTE)(GetFloat(session, m_actionTriggerLeft) * 255.0f);
    state.Gamepad.bRightTrigger = (BYTE)(GetFloat(session, m_actionTriggerRight) * 255.0f);
    
    XrVector2f thumbL = GetVector2(session, m_actionThumbstickLeft);
    XrVector2f thumbR = GetVector2(session, m_actionThumbstickRight);
    
    state.Gamepad.sThumbLX = (SHORT)(thumbL.x * 32767.0f);
    state.Gamepad.sThumbLY = (SHORT)(thumbL.y * 32767.0f);
    state.Gamepad.sThumbRX = (SHORT)(thumbR.x * 32767.0f);
    state.Gamepad.sThumbRY = (SHORT)(thumbR.y * 32767.0f);
    
    // Determine if user is actively using VR controllers
    bool isActive = state.Gamepad.wButtons != 0 || state.Gamepad.bLeftTrigger > 0 || state.Gamepad.bRightTrigger > 0 ||
                    abs(state.Gamepad.sThumbLX) > 2000 || abs(state.Gamepad.sThumbLY) > 2000 ||
                    abs(state.Gamepad.sThumbRX) > 2000 || abs(state.Gamepad.sThumbRY) > 2000;
                    
    InputHook::GetInstance().SetVRControllersActive(isActive);
    InputHook::GetInstance().UpdateEmulatedState(state);
}

} // namespace vrinject
