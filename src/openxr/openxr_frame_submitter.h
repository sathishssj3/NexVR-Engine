#pragma once

#include "openxr_types.h"
#include "openxr_health_monitor.h"
#include "openxr_swapchain_manager.h"
#include <d3d11.h>
#include <chrono>

namespace vrinject {
namespace openxr {

class OpenXRFrameSubmitter {
public:
    OpenXRFrameSubmitter(OpenXRHealthMonitor* healthMonitor);
    ~OpenXRFrameSubmitter() = default;

    // DX11 Phased Submission
    bool BeginAndAcquireDX11(
        XrSession session,
        XrSpace referenceSpace,
        OpenXRSwapchainManager* swapchainManager,
        XrPosef& outLeftPose, XrFovf& outLeftFov,
        XrPosef& outRightPose, XrFovf& outRightFov
    );

    bool ReleaseAndEndDX11(
        XrSession session,
        XrSpace referenceSpace,
        OpenXRSwapchainManager* swapchainManager,
        ID3D11DeviceContext* context,
        ID3D11Texture2D* leftEyeTex,
        ID3D11Texture2D* rightEyeTex
    );

    // DX12 Phased Submission
    bool BeginAndAcquireDX12(
        XrSession session,
        XrSpace referenceSpace,
        OpenXRSwapchainManager* swapchainManager,
        ID3D12Resource*& outLeftDest,
        ID3D12Resource*& outRightDest,
        XrPosef& outLeftPose, XrFovf& outLeftFov,
        XrPosef& outRightPose, XrFovf& outRightFov
    );

    bool ReleaseAndEndDX12(
        XrSession session,
        XrSpace referenceSpace,
        OpenXRSwapchainManager* swapchainManager,
        uint32_t leftWidth, uint32_t leftHeight,
        uint32_t rightWidth, uint32_t rightHeight
    );

    // Vulkan Phased Submission
    bool BeginAndAcquireVulkan(
        XrSession session,
        XrSpace referenceSpace,
        OpenXRSwapchainManager* swapchainManager,
        VkImage& outLeftDest,
        VkImage& outRightDest,
        XrPosef& outLeftPose, XrFovf& outLeftFov,
        XrPosef& outRightPose, XrFovf& outRightFov
    );

    bool ReleaseAndEndVulkan(
        XrSession session,
        XrSpace referenceSpace,
        OpenXRSwapchainManager* swapchainManager,
        uint32_t leftWidth, uint32_t leftHeight,
        uint32_t rightWidth, uint32_t rightHeight
    );

    SubmitterState GetState() const { return state_; }
    void ResetState() { state_ = SubmitterState::WAIT_FRAME; }

private:
    bool LocateViews(XrSession session, XrSpace referenceSpace, 
                     XrPosef& outLeftPose, XrFovf& outLeftFov,
                     XrPosef& outRightPose, XrFovf& outRightFov);

    OpenXRHealthMonitor* healthMonitor_ = nullptr;
    SubmitterState state_ = SubmitterState::WAIT_FRAME;

    XrFrameState currentFrameState_{XR_TYPE_FRAME_STATE};
    
    // Cached fallback poses in case of tracking loss
    XrPosef lastLeftPose_ = {{0, 0, 0, 1}, {0, 0, 0}};
    XrPosef lastRightPose_ = {{0, 0, 0, 1}, {0, 0, 0}};
    XrFovf lastLeftFov_ = {-0.8f, 0.8f, 0.8f, -0.8f};
    XrFovf lastRightFov_ = {-0.8f, 0.8f, 0.8f, -0.8f};

    uint32_t lastAcquiredLeftIndex_ = 0;
    uint32_t lastAcquiredRightIndex_ = 0;
};

} // namespace openxr
} // namespace vrinject
