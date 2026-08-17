#pragma once

#include <windows.h>
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#define XR_USE_GRAPHICS_API_D3D12
#define XR_USE_GRAPHICS_API_VULKAN
#include <d3d11.h>
#include <d3d12.h>
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <vector>

namespace vrinject {
namespace harness {

// The mock runtime intercepts OpenXR calls to allow testing the Golden Pipeline without a real headset.
class MockOpenXRRuntime {
public:
    static void Initialize();
    static void Shutdown();

    // Scripted pose parameters
    static void SetNextPose(const XrPosef& pose);
    
    // Test verification
    static const std::vector<XrCompositionLayerProjectionView>& GetSubmittedViews();

    static int g_eventCounter;
    static bool g_sessionCreated;

    // Detours for OpenXR functions
    static XrResult XRAPI_CALL Mock_xrCreateInstance(const XrInstanceCreateInfo* createInfo, XrInstance* instance);
    static XrResult XRAPI_CALL Mock_xrGetSystem(XrInstance instance, const XrSystemGetInfo* getInfo, XrSystemId* systemId);
    static XrResult XRAPI_CALL Mock_xrCreateSession(XrInstance instance, const XrSessionCreateInfo* createInfo, XrSession* session);
    static XrResult XRAPI_CALL Mock_xrBeginSession(XrSession session, const XrSessionBeginInfo* beginInfo);
    static XrResult XRAPI_CALL Mock_xrEndSession(XrSession session);
    static XrResult XRAPI_CALL Mock_xrCreateReferenceSpace(XrSession session, const XrReferenceSpaceCreateInfo* createInfo, XrSpace* space);
    static XrResult XRAPI_CALL Mock_xrEnumerateSwapchainFormats(XrSession session, uint32_t formatCapacityInput, uint32_t* formatCountOutput, int64_t* formats);
    static XrResult XRAPI_CALL Mock_xrCreateSwapchain(XrSession session, const XrSwapchainCreateInfo* createInfo, XrSwapchain* swapchain);
    static XrResult XRAPI_CALL Mock_xrEnumerateSwapchainImages(XrSwapchain swapchain, uint32_t imageCapacityInput, uint32_t* imageCountOutput, XrSwapchainImageBaseHeader* images);
    static XrResult XRAPI_CALL Mock_xrWaitFrame(XrSession session, const XrFrameWaitInfo* frameWaitInfo, XrFrameState* frameState);
    static XrResult XRAPI_CALL Mock_xrBeginFrame(XrSession session, const XrFrameBeginInfo* frameBeginInfo);
    static XrResult XRAPI_CALL Mock_xrLocateViews(XrSession session, const XrViewLocateInfo* viewLocateInfo, XrViewState* viewState, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrView* views);
    static XrResult XRAPI_CALL Mock_xrAcquireSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageAcquireInfo* acquireInfo, uint32_t* index);
    static XrResult XRAPI_CALL Mock_xrWaitSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageWaitInfo* waitInfo);
    static XrResult XRAPI_CALL Mock_xrReleaseSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageReleaseInfo* releaseInfo);
    static XrResult XRAPI_CALL Mock_xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo);
    static XrResult XRAPI_CALL Mock_xrGetInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* function);
    static XrResult XRAPI_CALL Mock_xrGetD3D11GraphicsRequirementsKHR(XrInstance instance, XrSystemId systemId, XrGraphicsRequirementsD3D11KHR* graphicsRequirements);
    static XrResult XRAPI_CALL Mock_xrPollEvent(XrInstance instance, XrEventDataBuffer* eventData);
    static XrResult XRAPI_CALL Mock_xrEnumerateReferenceSpaces(XrSession session, uint32_t spaceCapacityInput, uint32_t* spaceCountOutput, XrReferenceSpaceType* spaces);
    static XrResult XRAPI_CALL Mock_xrCreateActionSet(XrInstance instance, const XrActionSetCreateInfo* createInfo, XrActionSet* actionSet);
    static XrResult XRAPI_CALL Mock_xrDestroyActionSet(XrActionSet actionSet);
    static XrResult XRAPI_CALL Mock_xrCreateAction(XrActionSet actionSet, const XrActionCreateInfo* createInfo, XrAction* action);
    static XrResult XRAPI_CALL Mock_xrStringToPath(XrInstance instance, const char* pathString, XrPath* path);
    static XrResult XRAPI_CALL Mock_xrSuggestInteractionProfileBindings(XrInstance instance, const XrInteractionProfileSuggestedBinding* suggestedBindings);
    static XrResult XRAPI_CALL Mock_xrAttachSessionActionSets(XrSession session, const XrSessionActionSetsAttachInfo* attachInfo);
    static XrResult XRAPI_CALL Mock_xrGetActionStateBoolean(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateBoolean* state);
    static XrResult XRAPI_CALL Mock_xrGetActionStateFloat(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateFloat* state);
    static XrResult XRAPI_CALL Mock_xrGetActionStateVector2f(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateVector2f* state);
    static XrResult XRAPI_CALL Mock_xrSyncActions(XrSession session, const XrActionsSyncInfo* syncInfo);

    // Trampolines
    static decltype(xrCreateInstance)* True_xrCreateInstance;
    static decltype(xrGetSystem)* True_xrGetSystem;
    static decltype(xrCreateSession)* True_xrCreateSession;
    static decltype(xrBeginSession)* True_xrBeginSession;
    static decltype(xrEndSession)* True_xrEndSession;
    static decltype(xrCreateReferenceSpace)* True_xrCreateReferenceSpace;
    static decltype(xrEnumerateSwapchainFormats)* True_xrEnumerateSwapchainFormats;
    static decltype(xrCreateSwapchain)* True_xrCreateSwapchain;
    static decltype(xrEnumerateSwapchainImages)* True_xrEnumerateSwapchainImages;
    static decltype(xrWaitFrame)* True_xrWaitFrame;
    static decltype(xrBeginFrame)* True_xrBeginFrame;
    static decltype(xrLocateViews)* True_xrLocateViews;
    static decltype(xrAcquireSwapchainImage)* True_xrAcquireSwapchainImage;
    static decltype(xrWaitSwapchainImage)* True_xrWaitSwapchainImage;
    static decltype(xrReleaseSwapchainImage)* True_xrReleaseSwapchainImage;
    static decltype(xrEndFrame)* True_xrEndFrame;
    static decltype(xrGetInstanceProcAddr)* True_xrGetInstanceProcAddr;
    static PFN_xrGetD3D11GraphicsRequirementsKHR True_xrGetD3D11GraphicsRequirementsKHR;
    static decltype(xrPollEvent)* True_xrPollEvent;
    static decltype(xrEnumerateReferenceSpaces)* True_xrEnumerateReferenceSpaces;
    static decltype(xrCreateActionSet)* True_xrCreateActionSet;
    static decltype(xrDestroyActionSet)* True_xrDestroyActionSet;
    static decltype(xrCreateAction)* True_xrCreateAction;
    static decltype(xrStringToPath)* True_xrStringToPath;
    static decltype(xrSuggestInteractionProfileBindings)* True_xrSuggestInteractionProfileBindings;
    static decltype(xrAttachSessionActionSets)* True_xrAttachSessionActionSets;
    static decltype(xrGetActionStateBoolean)* True_xrGetActionStateBoolean;
    static decltype(xrGetActionStateFloat)* True_xrGetActionStateFloat;
    static decltype(xrGetActionStateVector2f)* True_xrGetActionStateVector2f;
    static decltype(xrSyncActions)* True_xrSyncActions;
};

} // namespace harness
} // namespace vrinject



