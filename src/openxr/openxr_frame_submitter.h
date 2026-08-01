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

    bool SubmitStereoTextures(
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
        OpenXRSwapchainManager* swapchainManager,
        ID3D12Resource*& outLeftDest,
        ID3D12Resource*& outRightDest
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
        OpenXRSwapchainManager* swapchainManager,
        VkImage& outLeftDest,
        VkImage& outRightDest
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
    OpenXRHealthMonitor* healthMonitor_ = nullptr;
    SubmitterState state_ = SubmitterState::WAIT_FRAME;

    XrFrameState currentFrameState_{XR_TYPE_FRAME_STATE};
};

} // namespace openxr
} // namespace vrinject
