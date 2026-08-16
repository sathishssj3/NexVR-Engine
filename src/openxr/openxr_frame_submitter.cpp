#include "openxr/openxr_frame_submitter.h"
#include <iostream>
#include <vector>

namespace vrinject {
namespace openxr {

OpenXRFrameSubmitter::OpenXRFrameSubmitter(OpenXRHealthMonitor* healthMonitor)
    : healthMonitor_(healthMonitor) {}

bool OpenXRFrameSubmitter::LocateViews(XrSession session, XrSpace referenceSpace, 
                                       XrPosef& outLeftPose, XrFovf& outLeftFov,
                                       XrPosef& outRightPose, XrFovf& outRightFov) {
    XrViewLocateInfo viewLocateInfo{XR_TYPE_VIEW_LOCATE_INFO};
    viewLocateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    viewLocateInfo.displayTime = currentFrameState_.predictedDisplayTime;
    viewLocateInfo.space = referenceSpace;

    XrViewState viewState{XR_TYPE_VIEW_STATE};
    uint32_t viewCountOutput;
    XrView views[2] = {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
    XrResult res = xrLocateViews(session, &viewLocateInfo, &viewState, 2, &viewCountOutput, views);

    if (XR_SUCCEEDED(res) && viewCountOutput == 2 &&
        (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) &&
        (viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT)) {
        
        outLeftPose = views[0].pose;
        outLeftFov = views[0].fov;
        outRightPose = views[1].pose;
        outRightFov = views[1].fov;

        lastLeftPose_ = outLeftPose;
        lastLeftFov_ = outLeftFov;
        lastRightPose_ = outRightPose;
        lastRightFov_ = outRightFov;
        return true;
    } else {
        // Fallback to last known good pose
        outLeftPose = lastLeftPose_;
        outLeftFov = lastLeftFov_;
        outRightPose = lastRightPose_;
        outRightFov = lastRightFov_;
        return false;
    }
}

bool OpenXRFrameSubmitter::BeginAndAcquireDX11(
    XrSession session,
    XrSpace referenceSpace,
    OpenXRSwapchainManager* swapchainManager,
    XrPosef& outLeftPose, XrFovf& outLeftFov,
    XrPosef& outRightPose, XrFovf& outRightFov) 
{
    // 1. Wait Frame
    state_ = SubmitterState::WAIT_FRAME;
    XrFrameWaitInfo waitInfo{XR_TYPE_FRAME_WAIT_INFO};
    XrResult res = xrWaitFrame(session, &waitInfo, &currentFrameState_);
    if (XR_FAILED(res)) {
        if (healthMonitor_) healthMonitor_->RecordFrameDropped();
        return false;
    }

    // 2. Begin Frame
    state_ = SubmitterState::BEGIN_FRAME;
    XrFrameBeginInfo beginInfo{XR_TYPE_FRAME_BEGIN_INFO};
    res = xrBeginFrame(session, &beginInfo);
    if (XR_FAILED(res)) {
        if (healthMonitor_) healthMonitor_->RecordFrameDropped();
        return false;
    }

    if (!currentFrameState_.shouldRender) {
        // App is not in focus or visible
        return true; 
    }

    // Locate views for head tracking
    LocateViews(session, referenceSpace, outLeftPose, outLeftFov, outRightPose, outRightFov);

    // 3. Acquire Images
    state_ = SubmitterState::ACQUIRE_IMAGES;
    uint32_t leftIndex = 0;
    uint32_t rightIndex = 0;
    if (!swapchainManager->AcquireImages(leftIndex, rightIndex)) {
        return false;
    }
    lastAcquiredLeftIndex_ = leftIndex;
    lastAcquiredRightIndex_ = rightIndex;

    // 4. Wait Images
    state_ = SubmitterState::WAIT_IMAGES;
    if (!swapchainManager->WaitImages()) {
        swapchainManager->ReleaseImages();
        return false;
    }

    return true;
}

bool OpenXRFrameSubmitter::ReleaseAndEndDX11(
    XrSession session,
    XrSpace referenceSpace,
    OpenXRSwapchainManager* swapchainManager,
    ID3D11DeviceContext* context,
    ID3D11Texture2D* leftEyeTex,
    ID3D11Texture2D* rightEyeTex) 
{
    auto startTime = std::chrono::high_resolution_clock::now();

    if (!currentFrameState_.shouldRender) {
        state_ = SubmitterState::END_FRAME;
        XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
        endInfo.displayTime = currentFrameState_.predictedDisplayTime;
        endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        endInfo.layerCount = 0;
        xrEndFrame(session, &endInfo);
        return true;
    }

    // 5. Copy Textures
    state_ = SubmitterState::COPY;
    ID3D11Texture2D* leftDest = swapchainManager->GetLeftSwapchainImage(lastAcquiredLeftIndex_);
    ID3D11Texture2D* rightDest = swapchainManager->GetRightSwapchainImage(lastAcquiredRightIndex_);

    if (leftEyeTex && leftDest) {
        context->CopyResource(leftDest, leftEyeTex);
    }
    if (rightEyeTex && rightDest) {
        context->CopyResource(rightDest, rightEyeTex);
    }

    // 6. Release Images
    state_ = SubmitterState::RELEASE;
    swapchainManager->ReleaseImages();

    // 7. End Frame
    state_ = SubmitterState::END_FRAME;
    
    // Create projection layer
    XrCompositionLayerProjectionView projectionViews[2] = {
        {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},
        {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}
    };

    auto extentOf = [](ID3D11Texture2D* image) -> XrExtent2Di {
        if (!image) return XrExtent2Di{0, 0};
        D3D11_TEXTURE2D_DESC desc = {};
        image->GetDesc(&desc);
        return XrExtent2Di{static_cast<int32_t>(desc.Width), static_cast<int32_t>(desc.Height)};
    };

    // Left Eye
    projectionViews[0].pose = lastLeftPose_;
    projectionViews[0].fov = lastLeftFov_;
    projectionViews[0].subImage.swapchain = swapchainManager->GetLeftSwapchain();
    projectionViews[0].subImage.imageRect.extent = extentOf(leftDest);

    // Right Eye
    projectionViews[1].pose = lastRightPose_;
    projectionViews[1].fov = lastRightFov_;
    projectionViews[1].subImage.swapchain = swapchainManager->GetRightSwapchain();
    projectionViews[1].subImage.imageRect.extent = extentOf(rightDest);

    XrCompositionLayerProjection projectionLayer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    projectionLayer.space = referenceSpace;
    projectionLayer.viewCount = 2;
    projectionLayer.views = projectionViews;

    const XrCompositionLayerBaseHeader* const layers[] = {
        reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projectionLayer)
    };

    XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
    endInfo.displayTime = currentFrameState_.predictedDisplayTime;
    endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    endInfo.layerCount = 1;
    endInfo.layers = layers;

    XrResult res = xrEndFrame(session, &endInfo);
    if (XR_FAILED(res)) {
        if (healthMonitor_) healthMonitor_->RecordFrameDropped();
        return false;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    float latencyMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();

    if (healthMonitor_) {
        healthMonitor_->RecordFrameSubmitted(latencyMs); 
    }

    return true;
}

bool OpenXRFrameSubmitter::BeginAndAcquireDX12(
    XrSession session,
    XrSpace referenceSpace,
    OpenXRSwapchainManager* swapchainManager,
    ID3D12Resource*& outLeftDest,
    ID3D12Resource*& outRightDest,
    XrPosef& outLeftPose, XrFovf& outLeftFov,
    XrPosef& outRightPose, XrFovf& outRightFov) 
{
    // 1. Wait Frame
    state_ = SubmitterState::WAIT_FRAME;
    XrFrameWaitInfo waitInfo{XR_TYPE_FRAME_WAIT_INFO};
    XrResult res = xrWaitFrame(session, &waitInfo, &currentFrameState_);
    if (XR_FAILED(res)) return false;

    // 2. Begin Frame
    state_ = SubmitterState::BEGIN_FRAME;
    XrFrameBeginInfo beginInfo{XR_TYPE_FRAME_BEGIN_INFO};
    res = xrBeginFrame(session, &beginInfo);
    if (XR_FAILED(res)) return false;

    if (!currentFrameState_.shouldRender) {
        state_ = SubmitterState::END_FRAME;
        XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
        endInfo.displayTime = currentFrameState_.predictedDisplayTime;
        endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        endInfo.layerCount = 0;
        xrEndFrame(session, &endInfo);
        return false;
    }

    // 3. Acquire Images
    state_ = SubmitterState::ACQUIRE_IMAGES;
    uint32_t leftIndex = 0;
    uint32_t rightIndex = 0;
    if (!swapchainManager->AcquireImages(leftIndex, rightIndex)) {
        XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
        endInfo.displayTime = currentFrameState_.predictedDisplayTime;
        endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        endInfo.layerCount = 0;
        xrEndFrame(session, &endInfo);
        return false;
    }

    // 4. Wait Images
    state_ = SubmitterState::WAIT_IMAGES;
    if (!swapchainManager->WaitImages()) {
        swapchainManager->ReleaseImages();
        XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
        endInfo.displayTime = currentFrameState_.predictedDisplayTime;
        endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        endInfo.layerCount = 0;
        xrEndFrame(session, &endInfo);
        return false;
    }

    outLeftDest = swapchainManager->GetLeftSwapchainImageDX12(leftIndex);
    outRightDest = swapchainManager->GetRightSwapchainImageDX12(rightIndex);
    return true;
}

bool OpenXRFrameSubmitter::ReleaseAndEndDX12(
    XrSession session,
    XrSpace referenceSpace,
    OpenXRSwapchainManager* swapchainManager,
    uint32_t leftWidth, uint32_t leftHeight,
    uint32_t rightWidth, uint32_t rightHeight)
{
    // 6. Release Images
    state_ = SubmitterState::RELEASE;
    swapchainManager->ReleaseImages();

    // 7. End Frame
    state_ = SubmitterState::END_FRAME;
    
    XrFovf fov = {-0.8f, 0.8f, 0.8f, -0.8f};

    XrCompositionLayerProjectionView projectionViews[2] = {
        {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},
        {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}
    };

    projectionViews[0].pose.orientation.w = 1.0f;
    projectionViews[0].fov = fov;
    projectionViews[0].subImage.swapchain = swapchainManager->GetLeftSwapchain();
    projectionViews[0].subImage.imageRect.extent.width = leftWidth;
    projectionViews[0].subImage.imageRect.extent.height = leftHeight;
    
    projectionViews[1].pose.orientation.w = 1.0f;
    projectionViews[1].fov = fov;
    projectionViews[1].subImage.swapchain = swapchainManager->GetRightSwapchain();
    projectionViews[1].subImage.imageRect.extent.width = rightWidth;
    projectionViews[1].subImage.imageRect.extent.height = rightHeight;

    XrCompositionLayerProjection projectionLayer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    projectionLayer.space = referenceSpace;
    projectionLayer.viewCount = 2;
    projectionLayer.views = projectionViews;

    const XrCompositionLayerBaseHeader* const layers[] = {
        reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projectionLayer)
    };

    XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
    endInfo.displayTime = currentFrameState_.predictedDisplayTime;
    endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    endInfo.layerCount = 1;
    endInfo.layers = layers;

    XrResult res = xrEndFrame(session, &endInfo);
    if (XR_FAILED(res)) {
        if (healthMonitor_) healthMonitor_->RecordFrameDropped();
        return false;
    }

    if (healthMonitor_) {
        // We'll track it from this point for now
        healthMonitor_->RecordFrameSubmitted(1.0f); // Default to 1.0f for DX12 for now unless we pass startTime
        
        if (healthMonitor_->GetFramesSubmitted() % 1000 == 0) {
            std::cout << "[OpenXR Telemetry] DX12 Frames Submitted: " << healthMonitor_->GetFramesSubmitted() 
                      << " | Predicted Display Time: " << currentFrameState_.predictedDisplayTime << "\n";
        }
    }

    return true;
}

bool OpenXRFrameSubmitter::BeginAndAcquireVulkan(
    XrSession session,
    XrSpace referenceSpace,
    OpenXRSwapchainManager* swapchainManager,
    VkImage& outLeftDest,
    VkImage& outRightDest,
    XrPosef& outLeftPose, XrFovf& outLeftFov,
    XrPosef& outRightPose, XrFovf& outRightFov) 
{
    // 1. Wait Frame
    state_ = SubmitterState::WAIT_FRAME;
    XrFrameWaitInfo waitInfo{XR_TYPE_FRAME_WAIT_INFO};
    XrResult res = xrWaitFrame(session, &waitInfo, &currentFrameState_);
    if (XR_FAILED(res)) return false;

    // 2. Begin Frame
    state_ = SubmitterState::BEGIN_FRAME;
    XrFrameBeginInfo beginInfo{XR_TYPE_FRAME_BEGIN_INFO};
    res = xrBeginFrame(session, &beginInfo);
    if (XR_FAILED(res)) return false;

    if (!currentFrameState_.shouldRender) {
        state_ = SubmitterState::END_FRAME;
        XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
        endInfo.displayTime = currentFrameState_.predictedDisplayTime;
        endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        endInfo.layerCount = 0;
        xrEndFrame(session, &endInfo);
        return false;
    }

    // 3. Acquire Images
    state_ = SubmitterState::ACQUIRE_IMAGES;
    uint32_t leftIndex = 0;
    uint32_t rightIndex = 0;
    if (!swapchainManager->AcquireImages(leftIndex, rightIndex)) {
        XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
        endInfo.displayTime = currentFrameState_.predictedDisplayTime;
        endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        endInfo.layerCount = 0;
        xrEndFrame(session, &endInfo);
        return false;
    }

    // 4. Wait Images
    state_ = SubmitterState::WAIT_IMAGES;
    if (!swapchainManager->WaitImages()) {
        swapchainManager->ReleaseImages();
        XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
        endInfo.displayTime = currentFrameState_.predictedDisplayTime;
        endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        endInfo.layerCount = 0;
        xrEndFrame(session, &endInfo);
        return false;
    }

    outLeftDest = swapchainManager->GetLeftSwapchainImageVulkan(leftIndex);
    outRightDest = swapchainManager->GetRightSwapchainImageVulkan(rightIndex);
    return true;
}

bool OpenXRFrameSubmitter::ReleaseAndEndVulkan(
    XrSession session,
    XrSpace referenceSpace,
    OpenXRSwapchainManager* swapchainManager,
    uint32_t leftWidth, uint32_t leftHeight,
    uint32_t rightWidth, uint32_t rightHeight)
{
    // 6. Release Images
    state_ = SubmitterState::RELEASE;
    swapchainManager->ReleaseImages();

    // 7. End Frame
    state_ = SubmitterState::END_FRAME;
    
    XrFovf fov = {-0.8f, 0.8f, 0.8f, -0.8f};

    XrCompositionLayerProjectionView projectionViews[2] = {
        {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},
        {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}
    };

    projectionViews[0].pose.orientation.w = 1.0f;
    projectionViews[0].fov = fov;
    projectionViews[0].subImage.swapchain = swapchainManager->GetLeftSwapchain();
    projectionViews[0].subImage.imageRect.extent.width = leftWidth;
    projectionViews[0].subImage.imageRect.extent.height = leftHeight;
    
    projectionViews[1].pose.orientation.w = 1.0f;
    projectionViews[1].fov = fov;
    projectionViews[1].subImage.swapchain = swapchainManager->GetRightSwapchain();
    projectionViews[1].subImage.imageRect.extent.width = rightWidth;
    projectionViews[1].subImage.imageRect.extent.height = rightHeight;

    XrCompositionLayerProjection projectionLayer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    projectionLayer.space = referenceSpace;
    projectionLayer.viewCount = 2;
    projectionLayer.views = projectionViews;

    const XrCompositionLayerBaseHeader* const layers[] = {
        reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projectionLayer)
    };

    XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
    endInfo.displayTime = currentFrameState_.predictedDisplayTime;
    endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    endInfo.layerCount = 1;
    endInfo.layers = layers;

    XrResult res = xrEndFrame(session, &endInfo);
    if (XR_FAILED(res)) {
        if (healthMonitor_) healthMonitor_->RecordFrameDropped();
        return false;
    }

    if (healthMonitor_) {
        healthMonitor_->RecordFrameSubmitted(1.0f);
        
        if (healthMonitor_->GetFramesSubmitted() % 1000 == 0) {
            std::cout << "[OpenXR Telemetry] Vulkan Frames Submitted: " << healthMonitor_->GetFramesSubmitted() 
                      << " | Predicted Display Time: " << currentFrameState_.predictedDisplayTime << "\n";
        }
    }

    return true;
}

} // namespace openxr
} // namespace vrinject

