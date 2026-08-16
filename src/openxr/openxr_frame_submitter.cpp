#include "openxr_frame_submitter.h"
#include <iostream>
#include <vector>

namespace vrinject {
namespace openxr {

OpenXRFrameSubmitter::OpenXRFrameSubmitter(OpenXRHealthMonitor* healthMonitor)
    : healthMonitor_(healthMonitor) {}

bool OpenXRFrameSubmitter::SubmitStereoTextures(
    XrSession session,
    XrSpace referenceSpace,
    OpenXRSwapchainManager* swapchainManager,
    ID3D11DeviceContext* context,
    ID3D11Texture2D* leftEyeTex,
    ID3D11Texture2D* rightEyeTex) 
{
    auto startTime = std::chrono::high_resolution_clock::now();

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
        // App is not in focus or visible, end frame without rendering
        state_ = SubmitterState::END_FRAME;
        XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
        endInfo.displayTime = currentFrameState_.predictedDisplayTime;
        endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        endInfo.layerCount = 0;
        xrEndFrame(session, &endInfo);
        return true;
    }

    // 3. Acquire Images
    state_ = SubmitterState::ACQUIRE_IMAGES;
    uint32_t leftIndex = 0;
    uint32_t rightIndex = 0;
    if (!swapchainManager->AcquireImages(leftIndex, rightIndex)) {
        // If acquire fails, we must still end frame but with no layers.
        // Actually, if acquire fails, it's a severe swapchain error, we return false.
        // The spec requires ending the frame if it was begun.
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

    // 5. Copy Textures
    state_ = SubmitterState::COPY;
    ID3D11Texture2D* leftDest = swapchainManager->GetLeftSwapchainImage(leftIndex);
    ID3D11Texture2D* rightDest = swapchainManager->GetRightSwapchainImage(rightIndex);

    // Note: Validation of format/mips/resolution should happen here before CopyResource to prevent DXGI crash.
    // Assuming StereoResourceManager & SwapchainManager match formats (DXGI_FORMAT_B8G8R8A8_UNORM_SRGB or similar).
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
    // Hardcode generic FOV for now; this should match the StereoCameraGenerator
    XrFovf fov = {-0.8f, 0.8f, 0.8f, -0.8f}; // Left, Right, Up, Down

    XrCompositionLayerProjectionView projectionViews[2] = {
        {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},
        {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}
    };

    // imageRect declares how much of the swapchain image the compositor samples.
    // This was a literal 100x100, so the runtime stretched the top-left 100x100
    // texels of each eye across the whole frustum. DX12 (:243-250) and Vulkan
    // (:368-375) already pass real dimensions; only DX11 was wrong. Take them
    // from the texture itself - it is the exact resource the compositor reads,
    // and OpenXRSwapchainManager exposes no dimension accessor.
    auto extentOf = [](ID3D11Texture2D* image) -> XrExtent2Di {
        if (!image) return XrExtent2Di{0, 0};
        D3D11_TEXTURE2D_DESC desc = {};
        image->GetDesc(&desc);
        return XrExtent2Di{static_cast<int32_t>(desc.Width), static_cast<int32_t>(desc.Height)};
    };

    // pose stays identity deliberately. The renderer is head-agnostic
    // (StereoCameraGenerator takes no pose input), so an identity pose declares a
    // world-locked panel at the reference-space origin - the honest presentation
    // of a head-agnostic render. Declaring a real XrView.pose here while the eye
    // matrices ignore it would head-lock the image and double-apply rotation.
    // Flip this only in the same commit that makes the render consume the pose.

    // Left Eye
    projectionViews[0].pose.orientation.w = 1.0f;
    projectionViews[0].fov = fov;
    projectionViews[0].subImage.swapchain = swapchainManager->GetLeftSwapchain();
    projectionViews[0].subImage.imageRect.extent = extentOf(swapchainManager->GetLeftSwapchainImage(0));

    // Right Eye
    projectionViews[1].pose.orientation.w = 1.0f;
    projectionViews[1].fov = fov;
    projectionViews[1].subImage.swapchain = swapchainManager->GetRightSwapchain();
    projectionViews[1].subImage.imageRect.extent = extentOf(swapchainManager->GetRightSwapchainImage(0));

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

    res = xrEndFrame(session, &endInfo);
    if (XR_FAILED(res)) {
        if (healthMonitor_) healthMonitor_->RecordFrameDropped();
        return false;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    float latencyMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();

    if (healthMonitor_) {
        // Track the delta between current time and predicted display time, or just internal submission latency
        healthMonitor_->RecordFrameSubmitted(latencyMs); 
        
        if (healthMonitor_->GetFramesSubmitted() % 1000 == 0) {
            std::cout << "[OpenXR Telemetry] Frames Submitted: " << healthMonitor_->GetFramesSubmitted() 
                      << " | Avg Submit Latency: " << healthMonitor_->GetAverageSubmitLatencyMs() << " ms"
                      << " | Predicted Display Time: " << currentFrameState_.predictedDisplayTime << "\n";
        }
    }

    return true;
}

bool OpenXRFrameSubmitter::BeginAndAcquireDX12(
    XrSession session,
    OpenXRSwapchainManager* swapchainManager,
    ID3D12Resource*& outLeftDest,
    ID3D12Resource*& outRightDest) 
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
    OpenXRSwapchainManager* swapchainManager,
    VkImage& outLeftDest,
    VkImage& outRightDest) 
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

