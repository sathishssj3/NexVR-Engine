#include "openxr_manager.h"
#include "../core/logger.h"
#include "../hooks/input_hook.h"
#include <vector>

#include "../rendering/backends/vulkan_renderer.h"

namespace vrinject {

OpenXRManager::OpenXRManager() {}

OpenXRManager::~OpenXRManager() {
    Shutdown();
}

bool OpenXRManager::Initialize(GraphicsAPI api, void* nativeDevice, void* nativeQueue) {
    if (!CreateInstance()) return false;
    if (!GetSystemId()) return false;
    if (!CreateSession(api, nativeDevice, nativeQueue)) return false;
    if (!CreateSwapchains()) return false;
    if (!CreateReferenceSpace()) return false;
    if (!InitializeActions()) return false;
    
    LOG_INFO("OpenXR Initialized successfully.");
    return true;
}

void OpenXRManager::Shutdown() {
    for (int i = 0; i < 2; ++i) {
        if (m_swapchains[i].handle != XR_NULL_HANDLE) {
            xrDestroySwapchain(m_swapchains[i].handle);
            m_swapchains[i].handle = XR_NULL_HANDLE;
            m_swapchains[i].images.clear();
        }
    }

    if (m_appSpace != XR_NULL_HANDLE) {
        xrDestroySpace(m_appSpace);
        m_appSpace = XR_NULL_HANDLE;
    }
    if (m_headSpace != XR_NULL_HANDLE) {
        xrDestroySpace(m_headSpace);
        m_headSpace = XR_NULL_HANDLE;
    }
    if (m_aimPoseSpace != XR_NULL_HANDLE) {
        xrDestroySpace(m_aimPoseSpace);
        m_aimPoseSpace = XR_NULL_HANDLE;
    }
    if (m_aimPoseAction != XR_NULL_HANDLE) {
        xrDestroyAction(m_aimPoseAction);
        m_aimPoseAction = XR_NULL_HANDLE;
    }
    if (m_session != XR_NULL_HANDLE) {
        xrDestroySession(m_session);
        m_session = XR_NULL_HANDLE;
    }
    if (m_instance != XR_NULL_HANDLE) {
        xrDestroyInstance(m_instance);
        m_instance = XR_NULL_HANDLE;
    }
}

bool OpenXRManager::CreateInstance() {
    XrInstanceCreateInfo createInfo = {XR_TYPE_INSTANCE_CREATE_INFO};
    createInfo.applicationInfo.applicationVersion = 1;
    strcpy_s(createInfo.applicationInfo.applicationName, "VRInject");
    createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
    
    const char* extensions[] = {
        XR_KHR_D3D11_ENABLE_EXTENSION_NAME,
        XR_KHR_D3D12_ENABLE_EXTENSION_NAME,
        XR_KHR_VULKAN_ENABLE_EXTENSION_NAME
    };
    createInfo.enabledExtensionCount = 3;
    createInfo.enabledExtensionNames = extensions;

    XrResult res = xrCreateInstance(&createInfo, &m_instance);
    if (XR_FAILED(res)) {
        LOG_ERROR("Failed to create OpenXR instance.");
        return false;
    }
    return true;
}

bool OpenXRManager::GetSystemId() {
    XrSystemGetInfo systemInfo = {XR_TYPE_SYSTEM_GET_INFO};
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    
    XrResult res = xrGetSystem(m_instance, &systemInfo, &m_systemId);
    if (XR_FAILED(res)) {
        LOG_ERROR("Failed to get OpenXR system.");
        return false;
    }
    return true;
}



bool OpenXRManager::CreateSession(GraphicsAPI api, void* nativeDevice, void* nativeQueue) {
    XrSessionCreateInfo sessionInfo = {XR_TYPE_SESSION_CREATE_INFO};

    XrGraphicsBindingD3D11KHR graphicsBindingDX11 = {XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
    XrGraphicsBindingD3D12KHR graphicsBindingDX12 = {XR_TYPE_GRAPHICS_BINDING_D3D12_KHR};

    if (api == GraphicsAPI::DX11) {
        PFN_xrGetD3D11GraphicsRequirementsKHR pfnGetD3D11GraphicsRequirementsKHR = nullptr;
        XrResult res = xrGetInstanceProcAddr(m_instance, "xrGetD3D11GraphicsRequirementsKHR", 
            (PFN_xrVoidFunction*)&pfnGetD3D11GraphicsRequirementsKHR);
        
        if (XR_SUCCEEDED(res) && pfnGetD3D11GraphicsRequirementsKHR) {
            XrGraphicsRequirementsD3D11KHR graphicsRequirements = {XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
            res = pfnGetD3D11GraphicsRequirementsKHR(m_instance, m_systemId, &graphicsRequirements);
            if (XR_FAILED(res)) {
                LOG_ERROR("Failed to query OpenXR D3D11 graphics requirements (Res: %d)", res);
                return false;
            }
        } else {
            LOG_ERROR("Failed to resolve xrGetD3D11GraphicsRequirementsKHR function pointer.");
            return false;
        }

        graphicsBindingDX11.device = static_cast<ID3D11Device*>(nativeDevice);
        sessionInfo.next = &graphicsBindingDX11;
    } else if (api == GraphicsAPI::DX12) {
        PFN_xrGetD3D12GraphicsRequirementsKHR pfnGetD3D12GraphicsRequirementsKHR = nullptr;
        XrResult res = xrGetInstanceProcAddr(m_instance, "xrGetD3D12GraphicsRequirementsKHR", 
            (PFN_xrVoidFunction*)&pfnGetD3D12GraphicsRequirementsKHR);
        
        if (XR_SUCCEEDED(res) && pfnGetD3D12GraphicsRequirementsKHR) {
            XrGraphicsRequirementsD3D12KHR graphicsRequirements = {XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR};
            res = pfnGetD3D12GraphicsRequirementsKHR(m_instance, m_systemId, &graphicsRequirements);
            if (XR_FAILED(res)) {
                LOG_ERROR("Failed to query OpenXR D3D12 graphics requirements (Res: %d)", res);
                return false;
            }
        } else {
            LOG_ERROR("Failed to resolve xrGetD3D12GraphicsRequirementsKHR function pointer.");
            return false;
        }

        graphicsBindingDX12.device = static_cast<ID3D12Device*>(nativeDevice);
        graphicsBindingDX12.queue = static_cast<ID3D12CommandQueue*>(nativeQueue);
        sessionInfo.next = &graphicsBindingDX12;
    } else if (api == GraphicsAPI::VULKAN) {
        PFN_xrGetVulkanGraphicsRequirements2KHR pfnGetVulkanGraphicsRequirements2KHR = nullptr;
        XrResult res = xrGetInstanceProcAddr(m_instance, "xrGetVulkanGraphicsRequirements2KHR", 
            (PFN_xrVoidFunction*)&pfnGetVulkanGraphicsRequirements2KHR);
        
        if (XR_SUCCEEDED(res) && pfnGetVulkanGraphicsRequirements2KHR) {
            XrGraphicsRequirementsVulkanKHR graphicsRequirements = {XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR};
            res = pfnGetVulkanGraphicsRequirements2KHR(m_instance, m_systemId, &graphicsRequirements);
            if (XR_FAILED(res)) {
                LOG_ERROR("Failed to query OpenXR Vulkan graphics requirements (Res: %d)", res);
                return false;
            }
        } else {
            LOG_ERROR("Failed to resolve xrGetVulkanGraphicsRequirements2KHR function pointer.");
            return false;
        }

        // For Vulkan, the nativeDevice needs to provide Instance, PhysicalDevice, and Device.
        // In our prototype architecture, VulkanRenderer has these.
        // We will retrieve them from the passed renderer object if needed, but for simplicity
        // we'll cast nativeDevice to VulkanRenderer* and extract them.
        VulkanRenderer* vkRenderer = static_cast<VulkanRenderer*>(m_renderer);
        if (!vkRenderer) {
            LOG_ERROR("m_renderer must be set before CreateSession for Vulkan");
            return false;
        }

        // We must define this statically so it lives for the duration of xrCreateSession
        static XrGraphicsBindingVulkanKHR graphicsBindingVK = {XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR};
        graphicsBindingVK.instance = vkRenderer->GetInstance();
        graphicsBindingVK.physicalDevice = vkRenderer->GetPhysicalDevice();
        graphicsBindingVK.device = vkRenderer->GetDevice();
        graphicsBindingVK.queueFamilyIndex = 0; // Stub
        graphicsBindingVK.queueIndex = 0;

        sessionInfo.next = &graphicsBindingVK;
    } else {
        LOG_ERROR("Unsupported GraphicsAPI for OpenXR Session creation.");
        return false;
    }

    sessionInfo.systemId = m_systemId;

    XrResult res = xrCreateSession(m_instance, &sessionInfo, &m_session);
    if (XR_FAILED(res)) {
        LOG_ERROR("Failed to create OpenXR session.");
        return false;
    }
    m_aimPredictor.Reset();
    return true;
}

bool OpenXRManager::CreateSwapchains() {
    uint32_t viewConfigCount = 0;
    if (XR_FAILED(xrEnumerateViewConfigurations(m_instance, m_systemId, 0, &viewConfigCount, nullptr))) return false;

    std::vector<XrViewConfigurationType> viewConfigs(viewConfigCount);
    if (XR_FAILED(xrEnumerateViewConfigurations(m_instance, m_systemId, viewConfigCount, &viewConfigCount, viewConfigs.data()))) return false;

    bool foundStereo = false;
    for (auto config : viewConfigs) {
        if (config == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) {
            foundStereo = true;
            break;
        }
    }
    if (!foundStereo) {
        LOG_ERROR("Stereo view configuration not supported by runtime.");
        return false;
    }

    uint32_t viewCount = 0;
    if (XR_FAILED(xrEnumerateViewConfigurationViews(m_instance, m_systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, nullptr))) return false;
    if (viewCount != 2) {
        LOG_ERROR("Expected 2 views for stereo, got %u", viewCount);
        return false;
    }

    m_viewConfigs[0] = {XR_TYPE_VIEW_CONFIGURATION_VIEW};
    m_viewConfigs[1] = {XR_TYPE_VIEW_CONFIGURATION_VIEW};
    if (XR_FAILED(xrEnumerateViewConfigurationViews(m_instance, m_systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2, &viewCount, m_viewConfigs))) return false;

    m_renderWidth = m_viewConfigs[0].recommendedImageRectWidth;
    m_renderHeight = m_viewConfigs[0].recommendedImageRectHeight;

    uint32_t formatCount = 0;
    if (XR_FAILED(xrEnumerateSwapchainFormats(m_session, 0, &formatCount, nullptr))) return false;

    std::vector<int64_t> formats(formatCount);
    if (XR_FAILED(xrEnumerateSwapchainFormats(m_session, formatCount, &formatCount, formats.data()))) return false;

    int64_t selectedFormat = -1;
    std::vector<int64_t> preferredFormats = {
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
        DXGI_FORMAT_B8G8R8A8_UNORM
    };

    for (int64_t runtimeFormat : formats) {
        for (int64_t preferredFormat : preferredFormats) {
            if (runtimeFormat == preferredFormat) {
                selectedFormat = runtimeFormat;
                break;
            }
        }
        if (selectedFormat != -1) break;
    }

    if (selectedFormat == -1) {
        LOG_ERROR("No supported swapchain format found.");
        return false;
    }
    
    LOG_INFO("Selected OpenXR swapchain format: %lld", selectedFormat);
    m_swapchainFormat = selectedFormat;

    for (int i = 0; i < 2; ++i) {
        XrSwapchainCreateInfo createInfo = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
        createInfo.arraySize = 1;
        createInfo.format = selectedFormat;
        createInfo.width = m_viewConfigs[i].recommendedImageRectWidth;
        createInfo.height = m_viewConfigs[i].recommendedImageRectHeight;
        createInfo.mipCount = 1;
        createInfo.faceCount = 1;
        createInfo.sampleCount = m_viewConfigs[i].recommendedSwapchainSampleCount;
        createInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;

        if (XR_FAILED(xrCreateSwapchain(m_session, &createInfo, &m_swapchains[i].handle))) {
            LOG_ERROR("Failed to create OpenXR swapchain for eye %d.", i);
            return false;
        }

        m_swapchains[i].width = createInfo.width;
        m_swapchains[i].height = createInfo.height;

        uint32_t imageCount = 0;
        if (XR_FAILED(xrEnumerateSwapchainImages(m_swapchains[i].handle, 0, &imageCount, nullptr))) return false;

        m_swapchains[i].images.resize(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
        if (XR_FAILED(xrEnumerateSwapchainImages(m_swapchains[i].handle, imageCount, &imageCount, (XrSwapchainImageBaseHeader*)m_swapchains[i].images.data()))) return false;
    }

    return true;
}

bool OpenXRManager::CreateReferenceSpace() {
    XrReferenceSpaceCreateInfo spaceInfo = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    spaceInfo.poseInReferenceSpace.orientation.w = 1.0f;
    
    XrResult res = xrCreateReferenceSpace(m_session, &spaceInfo, &m_appSpace);
    if (XR_FAILED(res)) {
        LOG_ERROR("Failed to create OpenXR reference space.");
        return false;
    }
    
    spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    res = xrCreateReferenceSpace(m_session, &spaceInfo, &m_headSpace);
    return XR_SUCCEEDED(res);
}

bool OpenXRManager::BeginFrame(XrFrameState& frameState) {
    XrFrameWaitInfo waitInfo = {XR_TYPE_FRAME_WAIT_INFO};
    if (XR_FAILED(xrWaitFrame(m_session, &waitInfo, &frameState))) return false;
    
    // Poll VR controllers to update the XInput emulation layer
    PollActions(frameState.predictedDisplayTime);

    XrFrameBeginInfo beginInfo = {XR_TYPE_FRAME_BEGIN_INFO};
    return XR_SUCCEEDED(xrBeginFrame(m_session, &beginInfo));
}

bool OpenXRManager::EndFrame(XrFrameState frameState, TextureHandle leftEye, TextureHandle rightEye) {
    XrViewLocateInfo viewLocateInfo = {XR_TYPE_VIEW_LOCATE_INFO};
    viewLocateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    viewLocateInfo.displayTime = frameState.predictedDisplayTime;
    viewLocateInfo.space = m_appSpace; // Usually located relative to app/reference space

    XrViewState viewState = {XR_TYPE_VIEW_STATE};
    uint32_t viewCount = 0;
    XrView views[2] = {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
    if (XR_FAILED(xrLocateViews(m_session, &viewLocateInfo, &viewState, 2, &viewCount, views))) return false;

    XrCompositionLayerProjectionView projectionViews[2] = {};
    TextureHandle eyeTextures[2] = {leftEye, rightEye};

    for (int i = 0; i < 2; ++i) {
        uint32_t imageIndex;
        XrSwapchainImageAcquireInfo acquireInfo = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        if (XR_FAILED(xrAcquireSwapchainImage(m_swapchains[i].handle, &acquireInfo, &imageIndex))) return false;

        XrSwapchainImageWaitInfo waitInfo = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        waitInfo.timeout = 100000000; // 100ms
        XrResult waitRes = xrWaitSwapchainImage(m_swapchains[i].handle, &waitInfo);
        if (waitRes == XR_TIMEOUT_EXPIRED) {
            LOG_WARN("Swapchain wait timed out — skipping frame");
            XrSwapchainImageReleaseInfo releaseInfo = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            xrReleaseSwapchainImage(m_swapchains[i].handle, &releaseInfo);
            return false;
        }
        if (XR_FAILED(waitRes)) return false;

        // Copy our rendered eye texture into the OpenXR swapchain texture
        if (eyeTextures[i].nativePtr && m_renderer) {
            m_renderer->CopyToSwapchain(eyeTextures[i], m_swapchains[i].images[imageIndex].texture);
        }

        XrSwapchainImageReleaseInfo releaseInfo = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        if (XR_FAILED(xrReleaseSwapchainImage(m_swapchains[i].handle, &releaseInfo))) return false;

        projectionViews[i].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
        projectionViews[i].pose = views[i].pose;
        projectionViews[i].fov = views[i].fov;
        projectionViews[i].subImage.swapchain = m_swapchains[i].handle;
        projectionViews[i].subImage.imageRect.offset.x = 0;
        projectionViews[i].subImage.imageRect.offset.y = 0;
        projectionViews[i].subImage.imageRect.extent.width = m_swapchains[i].width;
        projectionViews[i].subImage.imageRect.extent.height = m_swapchains[i].height;
        projectionViews[i].subImage.imageArrayIndex = 0;
    }

    XrCompositionLayerProjection layer = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    layer.space = m_appSpace; // Poses were located relative to appSpace
    layer.viewCount = 2;
    layer.views = projectionViews;

    const XrCompositionLayerBaseHeader* layers[1] = {reinterpret_cast<const XrCompositionLayerBaseHeader*>(&layer)};

    XrFrameEndInfo endInfo = {XR_TYPE_FRAME_END_INFO};
    endInfo.displayTime = frameState.predictedDisplayTime;
    endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    endInfo.layerCount = 1;
    endInfo.layers = layers;
    
    return XR_SUCCEEDED(xrEndFrame(m_session, &endInfo));
}

bool OpenXRManager::GetHeadPose(XrTime time, XrPosef& outPose) {
    XrSpaceLocation spaceLocation = {XR_TYPE_SPACE_LOCATION};
    if (XR_FAILED(xrLocateSpace(m_headSpace, m_appSpace, time, &spaceLocation))) return false;
    
    outPose = spaceLocation.pose;
    return (spaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0;
}

bool OpenXRManager::InitializeActions() {
    XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
    strcpy_s(actionSetInfo.actionSetName, "gameplay");
    strcpy_s(actionSetInfo.localizedActionSetName, "Gameplay");
    
    if (XR_FAILED(xrCreateActionSet(m_instance, &actionSetInfo, &m_actionSet))) {
        LOG_ERROR("OpenXR: Failed to create ActionSet");
        return false;
    }
    
    // Create aim pose action
    XrActionCreateInfo aimActionInfo{ XR_TYPE_ACTION_CREATE_INFO };
    aimActionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
    strcpy_s(aimActionInfo.actionName,     "right_aim_pose");
    strcpy_s(aimActionInfo.localizedActionName, "Right Hand Aim Pose");
    xrCreateAction(m_actionSet, &aimActionInfo, &m_aimPoseAction);
    
    auto CreateAction = [&](XrActionType type, const char* name, XrAction* outAction) {
        XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
        actionInfo.actionType = type;
        strcpy_s(actionInfo.actionName, name);
        strcpy_s(actionInfo.localizedActionName, name);
        xrCreateAction(m_actionSet, &actionInfo, outAction);
    };

    CreateAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "menu", &m_actionMenu);
    CreateAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "btn_a", &m_actionA);
    CreateAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "btn_b", &m_actionB);
    CreateAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "btn_x", &m_actionX);
    CreateAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "btn_y", &m_actionY);
    CreateAction(XR_ACTION_TYPE_FLOAT_INPUT, "trigger_l", &m_actionTriggerL);
    CreateAction(XR_ACTION_TYPE_FLOAT_INPUT, "trigger_r", &m_actionTriggerR);
    CreateAction(XR_ACTION_TYPE_VECTOR2F_INPUT, "thumbstick_l", &m_actionThumbstickL);
    CreateAction(XR_ACTION_TYPE_VECTOR2F_INPUT, "thumbstick_r", &m_actionThumbstickR);
    CreateAction(XR_ACTION_TYPE_VIBRATION_OUTPUT, "haptic", &m_actionHaptic);
    
    xrStringToPath(m_instance, "/user/hand/left", &m_handSubactionPath[0]);
    xrStringToPath(m_instance, "/user/hand/right", &m_handSubactionPath[1]);

    // Bindings for Oculus Touch (as a baseline example)
    XrPath profilePath;
    xrStringToPath(m_instance, "/interaction_profiles/oculus/touch_controller", &profilePath);
    
    auto GetPath = [&](const char* pathStr) {
        XrPath p; xrStringToPath(m_instance, pathStr, &p); return p;
    };

    std::vector<XrActionSuggestedBinding> bindings = {
        {m_actionA, GetPath("/user/hand/right/input/a/click")},
        {m_actionB, GetPath("/user/hand/right/input/b/click")},
        {m_actionX, GetPath("/user/hand/left/input/x/click")},
        {m_actionY, GetPath("/user/hand/left/input/y/click")},
        {m_actionMenu, GetPath("/user/hand/left/input/menu/click")},
        {m_actionTriggerL, GetPath("/user/hand/left/input/trigger/value")},
        {m_actionTriggerR, GetPath("/user/hand/right/input/trigger/value")},
        {m_actionThumbstickL, GetPath("/user/hand/left/input/thumbstick")},
        {m_actionThumbstickR, GetPath("/user/hand/right/input/thumbstick")},
        {m_aimPoseAction, GetPath("/user/hand/right/input/aim/pose")},
        {m_aimPoseAction, GetPath("/user/hand/right/input/grip/pose")},
        {m_actionHaptic, GetPath("/user/hand/left/output/haptic")},
        {m_actionHaptic, GetPath("/user/hand/right/output/haptic")}
    };

    XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    suggestedBindings.interactionProfile = profilePath;
    suggestedBindings.suggestedBindings = bindings.data();
    suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
    
    xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings);

    XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attachInfo.countActionSets = 1;
    attachInfo.actionSets = &m_actionSet;
    xrAttachSessionActionSets(m_session, &attachInfo);

    XrActionSpaceCreateInfo spaceInfo{ XR_TYPE_ACTION_SPACE_CREATE_INFO };
    spaceInfo.action            = m_aimPoseAction;
    spaceInfo.poseInActionSpace = { {0,0,0,1}, {0,0,0} }; // identity
    xrCreateActionSpace(m_session, &spaceInfo, &m_aimPoseSpace);

    LOG_INFO("OpenXR: Input actions initialized and bound.");
    return true;
}

void OpenXRManager::PollActions(XrTime displayTime) {
    if (m_session == XR_NULL_HANDLE || m_actionSet == XR_NULL_HANDLE) return;

    XrActiveActionSet activeActionSet{};
    activeActionSet.actionSet = m_actionSet;
    activeActionSet.subactionPath = XR_NULL_PATH;

    XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
    syncInfo.countActiveActionSets = 1;
    syncInfo.activeActionSets = &activeActionSet;

    if (XR_FAILED(xrSyncActions(m_session, &syncInfo))) return;

    m_aimPoseLocation = { XR_TYPE_SPACE_LOCATION };
    xrLocateSpace(m_aimPoseSpace,
                  m_appSpace,
                  displayTime,
                  &m_aimPoseLocation);

    if ((m_aimPoseLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) &&
        (m_aimPoseLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT)) {
        m_currentAimRotation = m_aimPoseLocation.pose.orientation;
    } else {
        m_aimPredictor.Reset();
    }

    auto GetBool = [&](XrAction action) -> bool {
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = action;
        XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
        xrGetActionStateBoolean(m_session, &getInfo, &state);
        return state.isActive && state.currentState;
    };
    
    auto GetFloat = [&](XrAction action) -> float {
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = action;
        XrActionStateFloat state{XR_TYPE_ACTION_STATE_FLOAT};
        xrGetActionStateFloat(m_session, &getInfo, &state);
        return state.isActive ? state.currentState : 0.0f;
    };

    auto GetVec2 = [&](XrAction action) -> XrVector2f {
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = action;
        XrActionStateVector2f state{XR_TYPE_ACTION_STATE_VECTOR2F};
        xrGetActionStateVector2f(m_session, &getInfo, &state);
        if (state.isActive) return state.currentState;
        return {0.0f, 0.0f};
    };

    XINPUT_STATE xstate = {};
    xstate.Gamepad.wButtons = 0;

    if (GetBool(m_actionA)) xstate.Gamepad.wButtons |= XINPUT_GAMEPAD_A;
    if (GetBool(m_actionB)) xstate.Gamepad.wButtons |= XINPUT_GAMEPAD_B;
    if (GetBool(m_actionX)) xstate.Gamepad.wButtons |= XINPUT_GAMEPAD_X;
    if (GetBool(m_actionY)) xstate.Gamepad.wButtons |= XINPUT_GAMEPAD_Y;
    if (GetBool(m_actionMenu)) xstate.Gamepad.wButtons |= XINPUT_GAMEPAD_START;

    xstate.Gamepad.bLeftTrigger = (BYTE)(GetFloat(m_actionTriggerL) * 255.0f);
    xstate.Gamepad.bRightTrigger = (BYTE)(GetFloat(m_actionTriggerR) * 255.0f);

    XrVector2f stickL = GetVec2(m_actionThumbstickL);
    XrVector2f stickR = GetVec2(m_actionThumbstickR);
    
    xstate.Gamepad.sThumbLX = (SHORT)(stickL.x * 32767.0f);
    xstate.Gamepad.sThumbLY = (SHORT)(stickL.y * 32767.0f);
    xstate.Gamepad.sThumbRX = (SHORT)(stickR.x * 32767.0f);
    xstate.Gamepad.sThumbRY = (SHORT)(stickR.y * 32767.0f);

    InputHook::GetInstance().UpdateEmulatedState(xstate);

    // 6DOF Motion Aiming Injection
    AimDelta aimDelta = m_aimPredictor.ComputeAimDelta(m_currentAimRotation);
    if (std::abs(aimDelta.pitchDeg) > 0.01f || std::abs(aimDelta.yawDeg) > 0.01f) {
        InputHook::GetInstance().InjectAimDelta(aimDelta.pitchDeg, aimDelta.yawDeg);
    }
}

void OpenXRManager::ApplyHapticFeedback(float leftMotor, float rightMotor) {
    if (m_session == XR_NULL_HANDLE || m_actionHaptic == XR_NULL_HANDLE) return;

    auto ApplyToHand = [&](float intensity, int handIndex) {
        if (intensity <= 0.0f) {
            XrHapticActionInfo hapticActionInfo{XR_TYPE_HAPTIC_ACTION_INFO};
            hapticActionInfo.action = m_actionHaptic;
            hapticActionInfo.subactionPath = m_handSubactionPath[handIndex];
            xrStopHapticFeedback(m_session, &hapticActionInfo);
            return;
        }
        XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
        vibration.amplitude = intensity;
        vibration.duration = XR_MIN_HAPTIC_DURATION;
        vibration.frequency = XR_FREQUENCY_UNSPECIFIED;

        XrHapticActionInfo hapticActionInfo{XR_TYPE_HAPTIC_ACTION_INFO};
        hapticActionInfo.action = m_actionHaptic;
        hapticActionInfo.subactionPath = m_handSubactionPath[handIndex];

        xrApplyHapticFeedback(m_session, &hapticActionInfo, (const XrHapticBaseHeader*)&vibration);
    };

    ApplyToHand(leftMotor, 0); // 0 is left
    ApplyToHand(rightMotor, 1); // 1 is right
}

} // namespace vrinject
