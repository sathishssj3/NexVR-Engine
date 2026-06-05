#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include "../core/motion_predictor.h"
#include "irenderer.h"
#include <d3d12.h>
#define VK_USE_PLATFORM_WIN32_KHR
#include "vulkan/vulkan.h"

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#define XR_USE_GRAPHICS_API_D3D12
#define XR_USE_GRAPHICS_API_VULKAN
#include "openxr/openxr.h"
#include "openxr/openxr_platform.h"

namespace vrinject {

class OpenXRManager {
public:
    OpenXRManager();
    ~OpenXRManager();

    bool Initialize(GraphicsAPI api, void* nativeDevice, void* nativeQueue = nullptr);
    void Shutdown();

    void SetRenderer(IRenderer* renderer) { m_renderer = renderer; }

    bool BeginFrame(XrFrameState& frameState);
    bool EndFrame(XrFrameState frameState, TextureHandle leftEye, TextureHandle rightEye);

    bool GetHeadPose(XrTime time, XrPosef& outPose);
    
    // Polling controller states and converting them to XINPUT_STATE
    void PollActions(XrTime displayTime);

    const XrQuaternionf& GetAimRotation() const { return m_currentAimRotation; }

    void ApplyHapticFeedback(float leftMotor, float rightMotor);

    uint32_t GetRenderWidth() const { return m_renderWidth; }
    uint32_t GetRenderHeight() const { return m_renderHeight; }

    bool IsSwapchainSRGB() const {
        return m_swapchainFormat == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || 
               m_swapchainFormat == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    }

private:
    bool CreateInstance();
    bool GetSystemId();
    bool CreateSession(GraphicsAPI api, void* nativeDevice, void* nativeQueue);
    bool CreateSwapchains();
    bool CreateReferenceSpace();
    bool InitializeActions();

    IRenderer* m_renderer = nullptr;

    XrInstance m_instance = XR_NULL_HANDLE;
    XrSystemId m_systemId = XR_NULL_SYSTEM_ID;
    XrSession m_session = XR_NULL_HANDLE;
    XrSpace m_appSpace = XR_NULL_HANDLE;
    XrSpace m_headSpace = XR_NULL_HANDLE;

    // Aim Pose Tracking
    XrAction          m_aimPoseAction      = XR_NULL_HANDLE;
    XrSpace           m_aimPoseSpace       = XR_NULL_HANDLE;
    XrSpaceLocation   m_aimPoseLocation    = { XR_TYPE_SPACE_LOCATION };
    XrQuaternionf     m_currentAimRotation = { 0.0f, 0.0f, 0.0f, 1.0f };
    MotionPredictor   m_aimPredictor;

    struct Swapchain {
        XrSwapchain handle;
        uint32_t width;
        uint32_t height;
        std::vector<XrSwapchainImageD3D11KHR> images;
    };

    Swapchain m_swapchains[2]; // Left and right eyes
    XrViewConfigurationView m_viewConfigs[2]; // Left and right eye view configs

    uint32_t m_renderWidth = 0;
    uint32_t m_renderHeight = 0;
    int64_t m_swapchainFormat = -1;

    // Input Action Handles
    XrActionSet m_actionSet = XR_NULL_HANDLE;
    XrAction m_actionMenu = XR_NULL_HANDLE;
    XrAction m_actionA = XR_NULL_HANDLE;
    XrAction m_actionB = XR_NULL_HANDLE;
    XrAction m_actionX = XR_NULL_HANDLE;
    XrAction m_actionY = XR_NULL_HANDLE;
    XrAction m_actionTriggerL = XR_NULL_HANDLE;
    XrAction m_actionTriggerR = XR_NULL_HANDLE;
    XrAction m_actionThumbstickL = XR_NULL_HANDLE;
    XrAction m_actionThumbstickR = XR_NULL_HANDLE;
    XrAction m_actionHaptic = XR_NULL_HANDLE;
    
    XrPath m_handSubactionPath[2]; // 0: Left, 1: Right
};

} // namespace vrinject
