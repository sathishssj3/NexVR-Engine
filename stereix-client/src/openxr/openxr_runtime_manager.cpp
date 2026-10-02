#include "openxr/openxr_runtime_manager.h"
#include <iostream>
#include <vector>
#include "core/logger.h"

// Since XR_USE_GRAPHICS_API_D3D11 is defined in openxr_types.h, OpenXR knows we need the D3D11 graphics requirement functions.
// We need to define XR_USE_GRAPHICS_API_D3D11 before including openxr.h, which openxr_types.h handles.

namespace vrinject {
namespace openxr {

OpenXRRuntimeManager::OpenXRRuntimeManager(OpenXRHealthMonitor* healthMonitor)
    : healthMonitor_(healthMonitor) {}

OpenXRRuntimeManager::~OpenXRRuntimeManager() {
    Shutdown();
}

bool OpenXRRuntimeManager::Initialize(const std::string& applicationName, GraphicsBackend backendAPI) {
    if (state_ != RuntimeState::UNINITIALIZED) {
        LOG_WARN("OpenXR: Initialize called but state is not UNINITIALIZED (state=%d)", (int)state_);
        return false;
    }

    LOG_INFO("OpenXR: Creating instance for app '%s' (backend=%d)...", applicationName.c_str(), (int)backendAPI);
    if (!CreateInstance(applicationName, backendAPI)) {
        LOG_ERROR("OpenXR: CreateInstance FAILED!");
        state_ = RuntimeState::FAILED;
        return false;
    }
    state_ = RuntimeState::INSTANCE_CREATED;
    LOG_INFO("OpenXR: Instance created successfully.");

    if (!GetSystem()) {
        LOG_ERROR("OpenXR: GetSystem FAILED! Is a VR headset connected and SteamVR running?");
        state_ = RuntimeState::FAILED;
        return false;
    }
    state_ = RuntimeState::SYSTEM_SELECTED;
    LOG_INFO("OpenXR: System selected (HMD found). State = SYSTEM_SELECTED.");
    
    return true;
}

bool OpenXRRuntimeManager::CreateInstance(const std::string& appName, GraphicsBackend backendAPI) {
    std::vector<const char*> extensions;
    
    if (backendAPI == GraphicsBackend::DX12) {
        extensions.push_back(XR_KHR_D3D12_ENABLE_EXTENSION_NAME);
    } else if (backendAPI == GraphicsBackend::Vulkan) {
        extensions.push_back(XR_KHR_VULKAN_ENABLE_EXTENSION_NAME);
    } else {
        extensions.push_back(XR_KHR_D3D11_ENABLE_EXTENSION_NAME);
    }

    XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
    strncpy_s(createInfo.applicationInfo.applicationName, appName.c_str(), XR_MAX_APPLICATION_NAME_SIZE - 1);
    createInfo.applicationInfo.applicationVersion = 1;
    strncpy_s(createInfo.applicationInfo.engineName, "NexVR", XR_MAX_ENGINE_NAME_SIZE - 1);
    createInfo.applicationInfo.engineVersion = 1;
    createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.enabledExtensionNames = extensions.data();

    XrResult res = xrCreateInstance(&createInfo, &instance_);
    if (XR_FAILED(res)) {
        LOG_ERROR("OpenXR: xrCreateInstance failed with result %d", (int)res);
        return false;
    }
    LOG_INFO("OpenXR: xrCreateInstance succeeded (instance=%p)", (void*)instance_);
    return true;
}

bool OpenXRRuntimeManager::GetSystem() {
    XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

    XrResult res = xrGetSystem(instance_, &systemInfo, &systemId_);
    if (XR_FAILED(res)) {
        LOG_ERROR("OpenXR: xrGetSystem failed with result %d", (int)res);
        return false;
    }
    LOG_INFO("OpenXR: xrGetSystem succeeded (systemId=%llu)", (unsigned long long)systemId_);
    return true;
}

bool OpenXRRuntimeManager::CreateSession(ID3D11Device* device) {
    if (state_ != RuntimeState::SYSTEM_SELECTED) {
        return false;
    }

    XrGraphicsBindingD3D11KHR graphicsBinding{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
    graphicsBinding.device = device;

    XrSessionCreateInfo createInfo{XR_TYPE_SESSION_CREATE_INFO};
    createInfo.next = &graphicsBinding;
    createInfo.systemId = systemId_;

    PFN_xrGetD3D11GraphicsRequirementsKHR pfnGetD3D11GraphicsRequirementsKHR = nullptr;
    xrGetInstanceProcAddr(instance_, "xrGetD3D11GraphicsRequirementsKHR", (PFN_xrVoidFunction*)(&pfnGetD3D11GraphicsRequirementsKHR));
    if (pfnGetD3D11GraphicsRequirementsKHR) {
        XrGraphicsRequirementsD3D11KHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
        pfnGetD3D11GraphicsRequirementsKHR(instance_, systemId_, &graphicsRequirements);
    }

    XrResult res = xrCreateSession(instance_, &createInfo, &session_);
    if (XR_FAILED(res)) {
        LOG_ERROR("OpenXR: xrCreateSession (DX11) failed with result %d", (int)res);
        return false;
    }
    LOG_INFO("OpenXR: DX11 session created successfully (session=%p)", (void*)session_);

    if (!CreateReferenceSpace()) {
        return false;
    }

    state_ = RuntimeState::SESSION_CREATED;
    return true;
}

bool OpenXRRuntimeManager::CheckDX12GraphicsRequirements(LUID* outAdapterLuid) {
    if (state_ != RuntimeState::SYSTEM_SELECTED || !outAdapterLuid) {
        return false;
    }

    PFN_xrGetD3D12GraphicsRequirementsKHR pfnGetD3D12GraphicsRequirementsKHR = nullptr;
    xrGetInstanceProcAddr(instance_, "xrGetD3D12GraphicsRequirementsKHR", (PFN_xrVoidFunction*)(&pfnGetD3D12GraphicsRequirementsKHR));
    if (!pfnGetD3D12GraphicsRequirementsKHR) {
        LOG_ERROR("OpenXR: Failed to get xrGetD3D12GraphicsRequirementsKHR function pointer.");
        return false;
    }

    XrGraphicsRequirementsD3D12KHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR};
    XrResult res = pfnGetD3D12GraphicsRequirementsKHR(instance_, systemId_, &graphicsRequirements);
    if (XR_FAILED(res)) {
        LOG_ERROR("OpenXR: xrGetD3D12GraphicsRequirementsKHR failed with result %d", (int)res);
        return false;
    }

    *outAdapterLuid = graphicsRequirements.adapterLuid;
    return true;
}

bool OpenXRRuntimeManager::CreateSessionDX12(ID3D12Device* device, ID3D12CommandQueue* commandQueue) {
    if (state_ != RuntimeState::SYSTEM_SELECTED) {
        return false;
    }

    XrGraphicsBindingD3D12KHR graphicsBinding{XR_TYPE_GRAPHICS_BINDING_D3D12_KHR};
    graphicsBinding.device = device;
    graphicsBinding.queue = commandQueue;

    XrSessionCreateInfo createInfo{XR_TYPE_SESSION_CREATE_INFO};
    createInfo.next = &graphicsBinding;
    createInfo.systemId = systemId_;

    XrResult res = xrCreateSession(instance_, &createInfo, &session_);
    if (XR_FAILED(res)) {
        LOG_ERROR("OpenXR: xrCreateSession (DX12) failed with result %d", (int)res);
        return false;
    }
    LOG_INFO("OpenXR: DX12 session created successfully (session=%p)", (void*)session_);

    if (!CreateReferenceSpace()) {
        return false;
    }

    state_ = RuntimeState::SESSION_CREATED;
    return true;
}

bool OpenXRRuntimeManager::CheckVulkanGraphicsRequirements(VkInstance vkInstance, VkPhysicalDevice* outPhysicalDevice) {
    if (state_ != RuntimeState::SYSTEM_SELECTED || !outPhysicalDevice) {
        return false;
    }

    PFN_xrGetVulkanGraphicsRequirementsKHR pfnGetVulkanGraphicsRequirementsKHR = nullptr;
    xrGetInstanceProcAddr(instance_, "xrGetVulkanGraphicsRequirementsKHR", (PFN_xrVoidFunction*)(&pfnGetVulkanGraphicsRequirementsKHR));
    if (!pfnGetVulkanGraphicsRequirementsKHR) {
        LOG_ERROR("OpenXR: Failed to get xrGetVulkanGraphicsRequirementsKHR function pointer.");
        return false;
    }

    XrGraphicsRequirementsVulkanKHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR};
    XrResult res = pfnGetVulkanGraphicsRequirementsKHR(instance_, systemId_, &graphicsRequirements);
    if (XR_FAILED(res)) {
        LOG_ERROR("OpenXR: xrGetVulkanGraphicsRequirementsKHR failed with result %d", (int)res);
        return false;
    }

    PFN_xrGetVulkanGraphicsDeviceKHR pfnGetVulkanGraphicsDeviceKHR = nullptr;
    xrGetInstanceProcAddr(instance_, "xrGetVulkanGraphicsDeviceKHR", (PFN_xrVoidFunction*)(&pfnGetVulkanGraphicsDeviceKHR));
    if (!pfnGetVulkanGraphicsDeviceKHR) {
        LOG_ERROR("OpenXR: Failed to get xrGetVulkanGraphicsDeviceKHR function pointer.");
        return false;
    }

    res = pfnGetVulkanGraphicsDeviceKHR(instance_, systemId_, vkInstance, outPhysicalDevice);
    if (XR_FAILED(res)) {
        LOG_ERROR("OpenXR: xrGetVulkanGraphicsDeviceKHR failed with result %d", (int)res);
        return false;
    }

    return true;
}

bool OpenXRRuntimeManager::GetVulkanInstanceExtensions(std::string& outExtensions) {
    if (state_ != RuntimeState::SYSTEM_SELECTED) return false;

    PFN_xrGetVulkanInstanceExtensionsKHR pfnGetVulkanInstanceExtensionsKHR = nullptr;
    xrGetInstanceProcAddr(instance_, "xrGetVulkanInstanceExtensionsKHR", (PFN_xrVoidFunction*)(&pfnGetVulkanInstanceExtensionsKHR));
    if (!pfnGetVulkanInstanceExtensionsKHR) return false;

    uint32_t extensionBufferSize = 0;
    XrResult res = pfnGetVulkanInstanceExtensionsKHR(instance_, systemId_, 0, &extensionBufferSize, nullptr);
    if (XR_FAILED(res)) return false;

    outExtensions.resize(extensionBufferSize);
    res = pfnGetVulkanInstanceExtensionsKHR(instance_, systemId_, extensionBufferSize, &extensionBufferSize, &outExtensions[0]);
    if (XR_FAILED(res)) return false;

    return true;
}

bool OpenXRRuntimeManager::GetVulkanDeviceExtensions(std::string& outExtensions) {
    if (state_ != RuntimeState::SYSTEM_SELECTED) return false;

    PFN_xrGetVulkanDeviceExtensionsKHR pfnGetVulkanDeviceExtensionsKHR = nullptr;
    xrGetInstanceProcAddr(instance_, "xrGetVulkanDeviceExtensionsKHR", (PFN_xrVoidFunction*)(&pfnGetVulkanDeviceExtensionsKHR));
    if (!pfnGetVulkanDeviceExtensionsKHR) return false;

    uint32_t extensionBufferSize = 0;
    XrResult res = pfnGetVulkanDeviceExtensionsKHR(instance_, systemId_, 0, &extensionBufferSize, nullptr);
    if (XR_FAILED(res)) return false;

    outExtensions.resize(extensionBufferSize);
    res = pfnGetVulkanDeviceExtensionsKHR(instance_, systemId_, extensionBufferSize, &extensionBufferSize, &outExtensions[0]);
    if (XR_FAILED(res)) return false;

    return true;
}

bool OpenXRRuntimeManager::CreateSessionVulkan(VkInstance vkInstance, VkPhysicalDevice physicalDevice, VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex) {
    if (state_ != RuntimeState::SYSTEM_SELECTED) {
        return false;
    }

    PFN_xrGetVulkanGraphicsDeviceKHR pfnGetVulkanGraphicsDeviceKHR = nullptr;
    xrGetInstanceProcAddr(instance_, "xrGetVulkanGraphicsDeviceKHR", (PFN_xrVoidFunction*)&pfnGetVulkanGraphicsDeviceKHR);
    if (pfnGetVulkanGraphicsDeviceKHR) {
        VkPhysicalDevice xrReqDevice = VK_NULL_HANDLE;
        XrResult devRes = pfnGetVulkanGraphicsDeviceKHR(instance_, systemId_, vkInstance, &xrReqDevice);
        if (XR_FAILED(devRes)) {
            LOG_WARN("OpenXR: xrGetVulkanGraphicsDeviceKHR failed with %d", (int)devRes);
        } else if (xrReqDevice != physicalDevice) {
            LOG_WARN("OpenXR: Requested physical device %p does not match application device %p!", (void*)xrReqDevice, (void*)physicalDevice);
            physicalDevice = xrReqDevice; // Force it to match what OpenXR wants!
        } else {
            LOG_INFO("OpenXR: xrGetVulkanGraphicsDeviceKHR validated physical device %p", (void*)physicalDevice);
        }
    } else {
        LOG_WARN("OpenXR: xrGetVulkanGraphicsDeviceKHR not found, session creation may fail");
    }

    PFN_xrGetVulkanGraphicsRequirementsKHR pfnGetVulkanGraphicsRequirementsKHR = nullptr;
    xrGetInstanceProcAddr(instance_, "xrGetVulkanGraphicsRequirementsKHR", (PFN_xrVoidFunction*)&pfnGetVulkanGraphicsRequirementsKHR);
    if (pfnGetVulkanGraphicsRequirementsKHR) {
        XrGraphicsRequirementsVulkanKHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR};
        pfnGetVulkanGraphicsRequirementsKHR(instance_, systemId_, &graphicsRequirements);
    } else {
        LOG_WARN("OpenXR: xrGetVulkanGraphicsRequirementsKHR not found, session creation may fail");
    }

    XrGraphicsBindingVulkanKHR graphicsBinding{XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR};
    graphicsBinding.instance = vkInstance;
    graphicsBinding.physicalDevice = physicalDevice;
    graphicsBinding.device = device;
    graphicsBinding.queueFamilyIndex = queueFamilyIndex;
    graphicsBinding.queueIndex = queueIndex;

    XrSessionCreateInfo createInfo{XR_TYPE_SESSION_CREATE_INFO};
    createInfo.next = &graphicsBinding;
    createInfo.systemId = systemId_;

    XrResult res = xrCreateSession(instance_, &createInfo, &session_);
    if (XR_FAILED(res)) {
        LOG_ERROR("OpenXR: xrCreateSession (Vulkan) failed with result %d", (int)res);
        return false;
    }
    LOG_INFO("OpenXR: Vulkan session created successfully (session=%p)", (void*)session_);

    if (!CreateReferenceSpace()) {
        return false;
    }

    state_ = RuntimeState::SESSION_CREATED;
    return true;
}

bool OpenXRRuntimeManager::CreateReferenceSpace() {
    uint32_t spaceCount = 0;
    xrEnumerateReferenceSpaces(session_, 0, &spaceCount, nullptr);
    std::vector<XrReferenceSpaceType> spaces(spaceCount);
    xrEnumerateReferenceSpaces(session_, spaceCount, &spaceCount, spaces.data());

    XrReferenceSpaceType preferredSpace = XR_REFERENCE_SPACE_TYPE_LOCAL;
    for (auto space : spaces) {
        if (space == XR_REFERENCE_SPACE_TYPE_STAGE) {
            preferredSpace = XR_REFERENCE_SPACE_TYPE_STAGE;
            break;
        }
    }

    XrReferenceSpaceCreateInfo createInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    createInfo.referenceSpaceType = preferredSpace;
    createInfo.poseInReferenceSpace.orientation.w = 1.0f;

    XrResult res = xrCreateReferenceSpace(session_, &createInfo, &referenceSpace_);
    if (XR_FAILED(res)) {
        LOG_ERROR("OpenXR: xrCreateReferenceSpace failed with result %d", (int)res);
        return false;
    }
    LOG_INFO("OpenXR: Reference space created successfully (Type: %s).", 
        preferredSpace == XR_REFERENCE_SPACE_TYPE_STAGE ? "STAGE" : "LOCAL");
    return true;
}

void OpenXRRuntimeManager::Shutdown() {
    if (referenceSpace_ != XR_NULL_HANDLE) {
        xrDestroySpace(referenceSpace_);
        referenceSpace_ = XR_NULL_HANDLE;
    }
    if (session_ != XR_NULL_HANDLE) {
        xrDestroySession(session_);
        session_ = XR_NULL_HANDLE;
    }
    if (instance_ != XR_NULL_HANDLE) {
        xrDestroyInstance(instance_);
        instance_ = XR_NULL_HANDLE;
    }
    state_ = RuntimeState::UNINITIALIZED;
}

void OpenXRRuntimeManager::PollEvents() {
    if (instance_ == XR_NULL_HANDLE) return;

    XrEventDataBuffer eventData{XR_TYPE_EVENT_DATA_BUFFER};
    
    while (xrPollEvent(instance_, &eventData) == XR_SUCCESS) {
        switch (eventData.type) {
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                auto* stateEvent = reinterpret_cast<XrEventDataSessionStateChanged*>(&eventData);
                HandleSessionStateChanged(*stateEvent);
                break;
            }
            case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
                LOG_WARN("OpenXR: Instance loss pending!");
                state_ = RuntimeState::FAILED;
                if (healthMonitor_) healthMonitor_->RecordRuntimeRestart();
                break;
            default:
                break;
        }
        eventData.type = XR_TYPE_EVENT_DATA_BUFFER; // Reset for next poll
    }
}

void OpenXRRuntimeManager::HandleSessionStateChanged(const XrEventDataSessionStateChanged& stateEvent) {
    if (stateEvent.session != session_) return;

    switch (stateEvent.state) {
        case XR_SESSION_STATE_READY:
            state_ = RuntimeState::SESSION_READY;
            LOG_INFO("OpenXR: Session state -> READY. Calling xrBeginSession...");
            {
                XrSessionBeginInfo beginInfo{XR_TYPE_SESSION_BEGIN_INFO};
                beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                XrResult beginRes = xrBeginSession(session_, &beginInfo);
                if (XR_FAILED(beginRes)) {
                    LOG_ERROR("OpenXR: xrBeginSession FAILED with result %d", (int)beginRes);
                } else {
                    LOG_INFO("OpenXR: xrBeginSession succeeded!");
                }
            }
            break;
        case XR_SESSION_STATE_SYNCHRONIZED:
        case XR_SESSION_STATE_VISIBLE:
            LOG_INFO("OpenXR: Session state -> VISIBLE/SYNCHRONIZED");
            state_ = RuntimeState::VISIBLE;
            break;
        case XR_SESSION_STATE_FOCUSED:
            LOG_INFO("OpenXR: Session state -> FOCUSED");
            state_ = RuntimeState::FOCUSED;
            break;
        case XR_SESSION_STATE_STOPPING:
            state_ = RuntimeState::STOPPING;
            xrEndSession(session_);
            break;
        case XR_SESSION_STATE_LOSS_PENDING:
            state_ = RuntimeState::LOSS_PENDING;
            if (healthMonitor_) healthMonitor_->RecordSessionLoss();
            break;
        case XR_SESSION_STATE_EXITING:
            state_ = RuntimeState::STOPPED;
            break;
        default:
            break;
    }
}

} // namespace openxr
} // namespace vrinject
