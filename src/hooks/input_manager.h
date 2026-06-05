#pragma once
#include <d3d11.h>
#include <string>

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#include "../rendering/openxr/openxr.h"

namespace vrinject {

class InputManager {
public:
    InputManager();
    ~InputManager();

    bool Initialize(XrInstance instance, XrSession session);
    void Update(XrSession session);

private:
    XrActionSet m_actionSet = XR_NULL_HANDLE;
    XrAction m_grabAction = XR_NULL_HANDLE;
    XrAction m_triggerAction = XR_NULL_HANDLE;
    XrAction m_poseAction = XR_NULL_HANDLE;
    
    XrSpace m_leftHandSpace = XR_NULL_HANDLE;
    XrSpace m_rightHandSpace = XR_NULL_HANDLE;
};

} // namespace vrinject
