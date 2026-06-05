#include "input_manager.h"
#include "../core/logger.h"

namespace vrinject {

InputManager::InputManager() {}

InputManager::~InputManager() {
    if (m_actionSet != XR_NULL_HANDLE) {
        xrDestroyActionSet(m_actionSet);
        m_actionSet = XR_NULL_HANDLE;
    }
}

bool InputManager::Initialize(XrInstance instance, XrSession session) {
    if (instance == XR_NULL_HANDLE || session == XR_NULL_HANDLE) {
        LOG_ERROR("InputManager: Invalid instance or session handle.");
        return false;
    }

    XrActionSetCreateInfo actionSetInfo = {XR_TYPE_ACTION_SET_CREATE_INFO};
    strcpy_s(actionSetInfo.actionSetName, "vrinject_input");
    strcpy_s(actionSetInfo.localizedActionSetName, "VRInject Input");
    
    XrResult res = xrCreateActionSet(instance, &actionSetInfo, &m_actionSet);
    if (XR_FAILED(res)) {
        LOG_ERROR("Failed to create OpenXR Action Set (Res: %d)", res);
        return false;
    }
    
    // Create actions
    XrActionCreateInfo actionInfo = {XR_TYPE_ACTION_CREATE_INFO};
    actionInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
    strcpy_s(actionInfo.actionName, "trigger");
    strcpy_s(actionInfo.localizedActionName, "Trigger");
    
    res = xrCreateAction(m_actionSet, &actionInfo, &m_triggerAction);
    if (XR_FAILED(res)) {
        LOG_ERROR("Failed to create OpenXR trigger action (Res: %d)", res);
        return false;
    }
    
    XrSessionActionSetsAttachInfo attachInfo = {XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attachInfo.countActionSets = 1;
    attachInfo.actionSets = &m_actionSet;
    res = xrAttachSessionActionSets(session, &attachInfo);
    if (XR_FAILED(res)) {
        LOG_ERROR("Failed to attach session action sets (Res: %d)", res);
        return false;
    }
    
    LOG_INFO("InputManager: OpenXR input actions initialized successfully.");
    return true;
}

void InputManager::Update(XrSession session) {
    if (session == XR_NULL_HANDLE || m_actionSet == XR_NULL_HANDLE) {
        return;
    }

    XrActiveActionSet activeActionSet = {m_actionSet, XR_NULL_PATH};
    XrActionsSyncInfo syncInfo = {XR_TYPE_ACTIONS_SYNC_INFO};
    syncInfo.countActiveActionSets = 1;
    syncInfo.activeActionSets = &activeActionSet;
    
    XrResult res = xrSyncActions(session, &syncInfo);
    if (XR_FAILED(res)) {
        return;
    }
    
    // Query action states here and map to SendInput (mouse/keyboard)
    XrActionStateFloat triggerState = {XR_TYPE_ACTION_STATE_FLOAT};
    XrActionStateGetInfo getInfo = {XR_TYPE_ACTION_STATE_GET_INFO};
    getInfo.action = m_triggerAction;
    
    res = xrGetActionStateFloat(session, &getInfo, &triggerState);
    if (XR_SUCCEEDED(res) && triggerState.isActive && triggerState.currentState > 0.5f) {
        static int clickCounter = 0;
        if (clickCounter++ % 90 == 0) {
            LOG_DEBUG("InputManager: Trigger pulled (value: %.2f)", triggerState.currentState);
        }
    }
}

} // namespace vrinject
