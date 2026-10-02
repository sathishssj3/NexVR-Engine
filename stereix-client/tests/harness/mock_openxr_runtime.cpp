#include "mock_openxr_runtime.h"
#include <MinHook.h>
#include <iostream>
#include <string.h>

namespace vrinject {
namespace harness {

decltype(xrCreateInstance)* MockOpenXRRuntime::True_xrCreateInstance = nullptr;
decltype(xrGetSystem)* MockOpenXRRuntime::True_xrGetSystem = nullptr;
decltype(xrCreateSession)* MockOpenXRRuntime::True_xrCreateSession = nullptr;
decltype(xrBeginSession)* MockOpenXRRuntime::True_xrBeginSession = nullptr;
decltype(xrEndSession)* MockOpenXRRuntime::True_xrEndSession = nullptr;
decltype(xrCreateReferenceSpace)* MockOpenXRRuntime::True_xrCreateReferenceSpace = nullptr;
decltype(xrEnumerateSwapchainFormats)* MockOpenXRRuntime::True_xrEnumerateSwapchainFormats = nullptr;
decltype(xrCreateSwapchain)* MockOpenXRRuntime::True_xrCreateSwapchain = nullptr;
decltype(xrEnumerateSwapchainImages)* MockOpenXRRuntime::True_xrEnumerateSwapchainImages = nullptr;
decltype(xrWaitFrame)* MockOpenXRRuntime::True_xrWaitFrame = nullptr;
decltype(xrBeginFrame)* MockOpenXRRuntime::True_xrBeginFrame = nullptr;
decltype(xrLocateViews)* MockOpenXRRuntime::True_xrLocateViews = nullptr;
decltype(xrAcquireSwapchainImage)* MockOpenXRRuntime::True_xrAcquireSwapchainImage = nullptr;
decltype(xrWaitSwapchainImage)* MockOpenXRRuntime::True_xrWaitSwapchainImage = nullptr;
decltype(xrReleaseSwapchainImage)* MockOpenXRRuntime::True_xrReleaseSwapchainImage = nullptr;
decltype(xrEndFrame)* MockOpenXRRuntime::True_xrEndFrame = nullptr;
PFN_xrGetD3D11GraphicsRequirementsKHR MockOpenXRRuntime::True_xrGetD3D11GraphicsRequirementsKHR = nullptr;
decltype(xrGetInstanceProcAddr)* MockOpenXRRuntime::True_xrGetInstanceProcAddr = nullptr;
decltype(xrPollEvent)* MockOpenXRRuntime::True_xrPollEvent = nullptr;
decltype(xrEnumerateReferenceSpaces)* MockOpenXRRuntime::True_xrEnumerateReferenceSpaces = nullptr;
decltype(xrCreateActionSet)* MockOpenXRRuntime::True_xrCreateActionSet = nullptr;
decltype(xrDestroyActionSet)* MockOpenXRRuntime::True_xrDestroyActionSet = nullptr;
decltype(xrCreateAction)* MockOpenXRRuntime::True_xrCreateAction = nullptr;
decltype(xrStringToPath)* MockOpenXRRuntime::True_xrStringToPath = nullptr;
decltype(xrSuggestInteractionProfileBindings)* MockOpenXRRuntime::True_xrSuggestInteractionProfileBindings = nullptr;
decltype(xrAttachSessionActionSets)* MockOpenXRRuntime::True_xrAttachSessionActionSets = nullptr;
decltype(xrGetActionStateBoolean)* MockOpenXRRuntime::True_xrGetActionStateBoolean = nullptr;
decltype(xrGetActionStateFloat)* MockOpenXRRuntime::True_xrGetActionStateFloat = nullptr;
decltype(xrGetActionStateVector2f)* MockOpenXRRuntime::True_xrGetActionStateVector2f = nullptr;
decltype(xrSyncActions)* MockOpenXRRuntime::True_xrSyncActions = nullptr;

static XrPosef g_currentPose = { {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f} }; // Identity
static std::vector<XrCompositionLayerProjectionView> g_submittedViews;

void MockOpenXRRuntime::Initialize() {
    MH_Initialize();

    auto hook = [&](void* target, void* mockFunc, void** trueFunc) {
        if (target) {
            MH_CreateHook(target, mockFunc, trueFunc);
        }
    };

    hook((void*)xrCreateInstance, (void*)Mock_xrCreateInstance, (void**)&True_xrCreateInstance);
    hook((void*)xrGetInstanceProcAddr, (void*)Mock_xrGetInstanceProcAddr, (void**)&True_xrGetInstanceProcAddr);
    // Other functions will be intercepted via xrGetInstanceProcAddr, but we hook them just in case they are called directly
    hook((void*)xrGetSystem, (void*)Mock_xrGetSystem, (void**)&True_xrGetSystem);
    hook((void*)xrCreateSession, (void*)Mock_xrCreateSession, (void**)&True_xrCreateSession);
    hook((void*)xrBeginSession, (void*)Mock_xrBeginSession, (void**)&True_xrBeginSession);
    hook((void*)xrEndSession, (void*)Mock_xrEndSession, (void**)&True_xrEndSession);
    hook((void*)xrCreateReferenceSpace, (void*)Mock_xrCreateReferenceSpace, (void**)&True_xrCreateReferenceSpace);
    hook((void*)xrEnumerateSwapchainFormats, (void*)Mock_xrEnumerateSwapchainFormats, (void**)&True_xrEnumerateSwapchainFormats);
    hook((void*)xrCreateSwapchain, (void*)Mock_xrCreateSwapchain, (void**)&True_xrCreateSwapchain);
    hook((void*)xrEnumerateSwapchainImages, (void*)Mock_xrEnumerateSwapchainImages, (void**)&True_xrEnumerateSwapchainImages);
    hook((void*)xrWaitFrame, (void*)Mock_xrWaitFrame, (void**)&True_xrWaitFrame);
    hook((void*)xrBeginFrame, (void*)Mock_xrBeginFrame, (void**)&True_xrBeginFrame);
    hook((void*)xrLocateViews, (void*)Mock_xrLocateViews, (void**)&True_xrLocateViews);
    hook((void*)xrAcquireSwapchainImage, (void*)Mock_xrAcquireSwapchainImage, (void**)&True_xrAcquireSwapchainImage);
    hook((void*)xrWaitSwapchainImage, (void*)Mock_xrWaitSwapchainImage, (void**)&True_xrWaitSwapchainImage);
    hook((void*)xrReleaseSwapchainImage, (void*)Mock_xrReleaseSwapchainImage, (void**)&True_xrReleaseSwapchainImage);
    hook((void*)xrEndFrame, (void*)Mock_xrEndFrame, (void**)&True_xrEndFrame);
    hook((void*)xrPollEvent, (void*)Mock_xrPollEvent, (void**)&True_xrPollEvent);
    hook((void*)xrEnumerateReferenceSpaces, (void*)Mock_xrEnumerateReferenceSpaces, (void**)&True_xrEnumerateReferenceSpaces);
    hook((void*)xrCreateActionSet, (void*)Mock_xrCreateActionSet, (void**)&True_xrCreateActionSet);
    hook((void*)xrDestroyActionSet, (void*)Mock_xrDestroyActionSet, (void**)&True_xrDestroyActionSet);
    hook((void*)xrCreateAction, (void*)Mock_xrCreateAction, (void**)&True_xrCreateAction);
    hook((void*)xrStringToPath, (void*)Mock_xrStringToPath, (void**)&True_xrStringToPath);
    hook((void*)xrSuggestInteractionProfileBindings, (void*)Mock_xrSuggestInteractionProfileBindings, (void**)&True_xrSuggestInteractionProfileBindings);
    hook((void*)xrAttachSessionActionSets, (void*)Mock_xrAttachSessionActionSets, (void**)&True_xrAttachSessionActionSets);
    hook((void*)xrGetActionStateBoolean, (void*)Mock_xrGetActionStateBoolean, (void**)&True_xrGetActionStateBoolean);
    hook((void*)xrGetActionStateFloat, (void*)Mock_xrGetActionStateFloat, (void**)&True_xrGetActionStateFloat);
    hook((void*)xrGetActionStateVector2f, (void*)Mock_xrGetActionStateVector2f, (void**)&True_xrGetActionStateVector2f);
    hook((void*)xrSyncActions, (void*)Mock_xrSyncActions, (void**)&True_xrSyncActions);

    MH_EnableHook(MH_ALL_HOOKS);
}

void MockOpenXRRuntime::Shutdown() {
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    g_submittedViews.clear();
    g_eventCounter = 0;
    g_sessionCreated = false;
}

void MockOpenXRRuntime::SetNextPose(const XrPosef& pose) {
    g_currentPose = pose;
}

const std::vector<XrCompositionLayerProjectionView>& MockOpenXRRuntime::GetSubmittedViews() {
    return g_submittedViews;
}

// --- Mock Implementations ---

XrResult XRAPI_CALL MockOpenXRRuntime::Mock_xrCreateInstance(const XrInstanceCreateInfo* createInfo, XrInstance* instance) {
    *instance = (XrInstance)0x1000;
    return XR_SUCCESS;
}

static XrResult XRAPI_CALL dummyD3D12Req(XrInstance, XrSystemId, void*) { return XR_SUCCESS; }
static XrResult XRAPI_CALL dummyVkReq(XrInstance, XrSystemId, void*) { return XR_SUCCESS; }
static XrResult XRAPI_CALL dummyVkDev(XrInstance, const void*, VkPhysicalDevice* vkDev) { *vkDev = (VkPhysicalDevice)0x1111; return XR_SUCCESS; }

XrResult XRAPI_CALL MockOpenXRRuntime::Mock_xrGetInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* function) {
    if (strcmp(name, "xrGetD3D11GraphicsRequirementsKHR") == 0) {
        *function = reinterpret_cast<PFN_xrVoidFunction>(Mock_xrGetD3D11GraphicsRequirementsKHR);
        return XR_SUCCESS;
    }
    if (strcmp(name, "xrGetD3D12GraphicsRequirementsKHR") == 0) {
        *function = reinterpret_cast<PFN_xrVoidFunction>(dummyD3D12Req);
        return XR_SUCCESS;
    }
    if (strcmp(name, "xrGetVulkanGraphicsRequirements2KHR") == 0) {
        *function = reinterpret_cast<PFN_xrVoidFunction>(dummyVkReq);
        return XR_SUCCESS;
    }
    if (strcmp(name, "xrGetVulkanGraphicsDevice2KHR") == 0) {
        *function = reinterpret_cast<PFN_xrVoidFunction>(dummyVkDev);
        return XR_SUCCESS;
    }

    if (True_xrGetInstanceProcAddr) {
        XrResult res = True_xrGetInstanceProcAddr(instance, name, function);
        if (XR_SUCCEEDED(res)) return res;
    }

    // Fallback if loader didn't find it
    if (strcmp(name, "xrCreateSwapchain") == 0) *function = reinterpret_cast<PFN_xrVoidFunction>(Mock_xrCreateSwapchain);
    else if (strcmp(name, "xrEnumerateSwapchainImages") == 0) *function = reinterpret_cast<PFN_xrVoidFunction>(Mock_xrEnumerateSwapchainImages);
    else return XR_ERROR_FUNCTION_UNSUPPORTED;
    
    return XR_SUCCESS;
}

XrResult XRAPI_CALL MockOpenXRRuntime::Mock_xrGetD3D11GraphicsRequirementsKHR(XrInstance instance, XrSystemId systemId, XrGraphicsRequirementsD3D11KHR* graphicsRequirements) {
    graphicsRequirements->adapterLuid.LowPart = 0;
    graphicsRequirements->adapterLuid.HighPart = 0;
    graphicsRequirements->minFeatureLevel = D3D_FEATURE_LEVEL_11_0;
    return XR_SUCCESS;
}

XrResult XRAPI_CALL MockOpenXRRuntime::Mock_xrGetSystem(XrInstance instance, const XrSystemGetInfo* getInfo, XrSystemId* systemId) {
    *systemId = (XrSystemId)0x2000;
    return XR_SUCCESS;
}

XrResult XRAPI_CALL MockOpenXRRuntime::Mock_xrCreateSession(XrInstance instance, const XrSessionCreateInfo* createInfo, XrSession* session) {
    *session = (XrSession)0x3000;
    g_sessionCreated = true;
    return XR_SUCCESS;
}

XrResult XRAPI_CALL MockOpenXRRuntime::Mock_xrBeginSession(XrSession session, const XrSessionBeginInfo* beginInfo) {
    return XR_SUCCESS;
}

XrResult XRAPI_CALL MockOpenXRRuntime::Mock_xrEndSession(XrSession session) {
    return XR_SUCCESS;
}

XrResult XRAPI_CALL MockOpenXRRuntime::Mock_xrEnumerateSwapchainImages(XrSwapchain swapchain, uint32_t imageCapacityInput, uint32_t* imageCountOutput, XrSwapchainImageBaseHeader* images) {
    if (imageCountOutput) *imageCountOutput = 3;
    if (images && imageCapacityInput >= 3) {
        // Just return dummy images
        XrSwapchainImageD3D11KHR* d3dImages = reinterpret_cast<XrSwapchainImageD3D11KHR*>(images);
        for (uint32_t i = 0; i < 3; i++) {
            d3dImages[i].type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
            d3dImages[i].next = nullptr;
            d3dImages[i].texture = nullptr;
        }
    }
    return XR_SUCCESS;
}

XrResult XRAPI_CALL MockOpenXRRuntime::Mock_xrCreateReferenceSpace(XrSession session, const XrReferenceSpaceCreateInfo* createInfo, XrSpace* space) {
    *space = (XrSpace)0x4000;
    return XR_SUCCESS;
}

XrResult XRAPI_CALL MockOpenXRRuntime::Mock_xrEnumerateSwapchainFormats(XrSession session, uint32_t formatCapacityInput, uint32_t* formatCountOutput, int64_t* formats) {
    if (formatCountOutput) *formatCountOutput = 1;
    if (formats && formatCapacityInput > 0) formats[0] = 28; // DXGI_FORMAT_R8G8B8A8_UNORM
    return XR_SUCCESS;
}

XrResult XRAPI_CALL MockOpenXRRuntime::Mock_xrCreateSwapchain(XrSession session, const XrSwapchainCreateInfo* createInfo, XrSwapchain* swapchain) {
    *swapchain = (XrSwapchain)0x5000;
    return XR_SUCCESS;
}

// Removed duplicate function

XrResult XRAPI_CALL MockOpenXRRuntime::Mock_xrWaitFrame(XrSession session, const XrFrameWaitInfo* frameWaitInfo, XrFrameState* frameState) {
    frameState->predictedDisplayTime = 1000000; // arbitrary future time
    frameState->predictedDisplayPeriod = 11111111; // 90Hz
    frameState->shouldRender = XR_TRUE;
    return XR_SUCCESS;
}

XrResult XRAPI_CALL MockOpenXRRuntime::Mock_xrBeginFrame(XrSession session, const XrFrameBeginInfo* frameBeginInfo) {
    return XR_SUCCESS;
}

XrResult XRAPI_CALL MockOpenXRRuntime::Mock_xrLocateViews(XrSession session, const XrViewLocateInfo* viewLocateInfo, XrViewState* viewState, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrView* views) {
    if (viewCountOutput) *viewCountOutput = 2;
    if (views && viewCapacityInput >= 2) {
        views[0].pose = g_currentPose; 
        views[0].pose.position.x -= 0.032f; 
        views[0].fov = { -0.8f, 0.8f, 0.8f, -0.8f }; 
        
        views[1].pose = g_currentPose;
        views[1].pose.position.x += 0.032f;
        views[1].fov = { -0.8f, 0.8f, 0.8f, -0.8f };
    }
    viewState->viewStateFlags = XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT;
    return XR_SUCCESS;
}

XrResult XRAPI_CALL MockOpenXRRuntime::Mock_xrAcquireSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageAcquireInfo* acquireInfo, uint32_t* index) {
    *index = 0;
    return XR_SUCCESS;
}

XrResult XRAPI_CALL MockOpenXRRuntime::Mock_xrWaitSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageWaitInfo* waitInfo) {
    return XR_SUCCESS;
}

XrResult XRAPI_CALL MockOpenXRRuntime::Mock_xrReleaseSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageReleaseInfo* releaseInfo) {
    return XR_SUCCESS;
}

XrResult XRAPI_CALL MockOpenXRRuntime::Mock_xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo) {
    g_submittedViews.clear();
    for (uint32_t i = 0; i < frameEndInfo->layerCount; ++i) {
        if (frameEndInfo->layers[i]->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION) {
            const XrCompositionLayerProjection* proj = reinterpret_cast<const XrCompositionLayerProjection*>(frameEndInfo->layers[i]);
            for (uint32_t v = 0; v < proj->viewCount; ++v) {
                g_submittedViews.push_back(proj->views[v]);
            }
        }
    }
    return XR_SUCCESS;
}

int MockOpenXRRuntime::g_eventCounter = 0;
bool MockOpenXRRuntime::g_sessionCreated = false;

XrResult XRAPI_CALL MockOpenXRRuntime::Mock_xrPollEvent(XrInstance instance, XrEventDataBuffer* eventData) {
    if (!g_sessionCreated) return XR_EVENT_UNAVAILABLE;

    if (g_eventCounter == 0) {
        auto* stateEvent = reinterpret_cast<XrEventDataSessionStateChanged*>(eventData);
        stateEvent->type = XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED;
        stateEvent->session = (XrSession)0x3000; // Mock session
        stateEvent->state = XR_SESSION_STATE_READY;
        stateEvent->time = 1000000;
        g_eventCounter++;
        return XR_SUCCESS;
    } else if (g_eventCounter == 1) {
        auto* stateEvent = reinterpret_cast<XrEventDataSessionStateChanged*>(eventData);
        stateEvent->type = XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED;
        stateEvent->session = (XrSession)0x3000; // Mock session
        stateEvent->state = XR_SESSION_STATE_FOCUSED;
        stateEvent->time = 1000001;
        g_eventCounter++;
        return XR_SUCCESS;
    }
    return XR_EVENT_UNAVAILABLE;
}

XrResult XRAPI_CALL MockOpenXRRuntime::Mock_xrEnumerateReferenceSpaces(XrSession session, uint32_t spaceCapacityInput, uint32_t* spaceCountOutput, XrReferenceSpaceType* spaces) {
    if (spaceCountOutput) *spaceCountOutput = 1;
    if (spaces && spaceCapacityInput > 0) spaces[0] = XR_REFERENCE_SPACE_TYPE_LOCAL;
    return XR_SUCCESS;
}

XrResult XRAPI_CALL MockOpenXRRuntime::Mock_xrCreateActionSet(XrInstance instance, const XrActionSetCreateInfo* createInfo, XrActionSet* actionSet) {
    *actionSet = (XrActionSet)0x5000;
    return XR_SUCCESS;
}

XrResult XRAPI_CALL MockOpenXRRuntime::Mock_xrDestroyActionSet(XrActionSet actionSet) {
    return XR_SUCCESS;
}

XrResult XRAPI_CALL MockOpenXRRuntime::Mock_xrCreateAction(XrActionSet actionSet, const XrActionCreateInfo* createInfo, XrAction* action) {
    *action = (XrAction)0x6000;
    return XR_SUCCESS;
}

XrResult XRAPI_CALL MockOpenXRRuntime::Mock_xrStringToPath(XrInstance instance, const char* pathString, XrPath* path) {
    // Generate a dummy path based on string hash or just a constant
    *path = (XrPath)0x7000;
    return XR_SUCCESS;
}

XrResult XRAPI_CALL MockOpenXRRuntime::Mock_xrSuggestInteractionProfileBindings(XrInstance instance, const XrInteractionProfileSuggestedBinding* suggestedBindings) {
    return XR_SUCCESS;
}

XrResult XRAPI_CALL MockOpenXRRuntime::Mock_xrAttachSessionActionSets(XrSession session, const XrSessionActionSetsAttachInfo* attachInfo) {
    return XR_SUCCESS;
}

XrResult XRAPI_CALL MockOpenXRRuntime::Mock_xrGetActionStateBoolean(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateBoolean* state) {
    state->isActive = XR_TRUE;
    state->currentState = XR_FALSE;
    state->changedSinceLastSync = XR_FALSE;
    return XR_SUCCESS;
}

XrResult XRAPI_CALL MockOpenXRRuntime::Mock_xrGetActionStateFloat(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateFloat* state) {
    state->isActive = XR_TRUE;
    state->currentState = 0.0f;
    state->changedSinceLastSync = XR_FALSE;
    return XR_SUCCESS;
}

XrResult XRAPI_CALL MockOpenXRRuntime::Mock_xrGetActionStateVector2f(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateVector2f* state) {
    state->isActive = XR_TRUE;
    state->currentState = {0.0f, 0.0f};
    state->changedSinceLastSync = XR_FALSE;
    return XR_SUCCESS;
}

XrResult XRAPI_CALL MockOpenXRRuntime::Mock_xrSyncActions(XrSession session, const XrActionsSyncInfo* syncInfo) {
    return XR_SUCCESS;
}

} // namespace harness
} // namespace vrinject
