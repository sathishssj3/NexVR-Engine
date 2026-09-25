#pragma once
#include <d3d11.h>
#include <string>

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#include "rendering/openxr/openxr.h"

namespace vrinject {

class InputManager {
public:
    InputManager();
    ~InputManager();

    bool Initialize(XrInstance instance, XrSession session);
    void Update(XrSession session);

private:
    XrActionSet m_actionSet = XR_NULL_HANDLE;
    
    // Boolean input actions
    XrAction m_actionMenu = XR_NULL_HANDLE;
    XrAction m_actionA = XR_NULL_HANDLE;
    XrAction m_actionB = XR_NULL_HANDLE;
    XrAction m_actionX = XR_NULL_HANDLE;
    XrAction m_actionY = XR_NULL_HANDLE;
    
    XrAction m_actionThumbstickClickLeft = XR_NULL_HANDLE;
    XrAction m_actionThumbstickClickRight = XR_NULL_HANDLE;
    
    // Float input actions (Triggers & Grips)
    XrAction m_actionTriggerLeft = XR_NULL_HANDLE;
    XrAction m_actionTriggerRight = XR_NULL_HANDLE;
    
    XrAction m_actionGripLeft = XR_NULL_HANDLE;
    XrAction m_actionGripRight = XR_NULL_HANDLE;
    
    // Vector2 input actions (Thumbsticks)
    XrAction m_actionThumbstickLeft = XR_NULL_HANDLE;
    XrAction m_actionThumbstickRight = XR_NULL_HANDLE;
    
    // Pose action for controller tracking
    XrAction m_poseAction = XR_NULL_HANDLE;
    
    // P0.1: Haptic output actions for two-way rumble
    XrAction m_actionHapticLeft = XR_NULL_HANDLE;
    XrAction m_actionHapticRight = XR_NULL_HANDLE;
    
    // Controller spaces
    XrSpace m_leftHandSpace = XR_NULL_HANDLE;
    XrSpace m_rightHandSpace = XR_NULL_HANDLE;

    // P0.1: Haptic feedback helper
    void ApplyHaptics(XrSession session, float leftAmplitude, float rightAmplitude);

    // P0.1: D-Pad emulation state
    bool m_leftStickClickHeld = false;
};

} // namespace vrinject
