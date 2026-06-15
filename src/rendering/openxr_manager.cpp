#include "openxr_manager.h"
#include "../core/logger.h"
#include "../core/matrix_classifier.h"
#include "../core/config_manager.h"
#include "../hooks/dx11_hook.h"
#include "../hooks/input_hook.h"
#include <vector>
#include <cmath>

#include "../rendering/backends/vulkan_renderer.h"
#include "stereo_pipeline.h"
#include "../rendering/backends/dx12_renderer.h"

namespace vrinject {

static OpenXRManager* s_instance = nullptr;

OpenXRManager::OpenXRManager() {
    s_instance = this;
}

OpenXRManager* OpenXRManager::GetInstance() {
    return s_instance;
}

OpenXRManager::~OpenXRManager() {
    Shutdown();
    if (m_instance != XR_NULL_HANDLE) {
        xrDestroyInstance(m_instance);
        m_instance = XR_NULL_HANDLE;
    }
}

bool OpenXRManager::Initialize(GraphicsAPI api, void* nativeDevice, void* nativeQueue, uint32_t targetWidth, uint32_t targetHeight, int64_t gameFormat) {
    m_api = api;
    if (m_instance == XR_NULL_HANDLE && !CreateInstance()) return false;
    if (m_systemId == XR_NULL_SYSTEM_ID && !GetSystemId()) return false;
    if (!CreateSession(api, nativeDevice, nativeQueue)) return false;
    if (!CreateSwapchains(targetWidth, targetHeight, gameFormat)) return false;
    if (!CreateReferenceSpace()) return false;
    if (!InitializeActions()) return false;
    
    m_vrThreadRunning = true;
    m_vrThread = std::thread(&OpenXRManager::VRRenderLoop, this);
    
    LOG_INFO("OpenXR Initialized successfully.");
    return true;
}

void OpenXRManager::Shutdown() {
    m_vrThreadRunning = false;
    if (m_vrThread.joinable()) {
        m_vrThread.join();
    }

    for (int i = 0; i < 2; ++i) {
        if (m_swapchains[i].handle != XR_NULL_HANDLE) {
            xrDestroySwapchain(m_swapchains[i].handle);
            m_swapchains[i].handle = XR_NULL_HANDLE;
            m_swapchains[i].images.clear();
        }
        m_crossAdapterCopiers[i].Shutdown();
    }

    if (m_proxyContext) {
        m_proxyContext->Release();
        m_proxyContext = nullptr;
    }
    if (m_proxyDevice) {
        m_proxyDevice->Release();
        m_proxyDevice = nullptr;
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
    // We intentionally DO NOT destroy m_instance here. OpenXR runtimes will often reject
    // rapid recreation of an XrInstance within the same process. It will be cleaned up by the destructor.
}

bool OpenXRManager::CreateInstance() {
    XrInstanceCreateInfo createInfo = {XR_TYPE_INSTANCE_CREATE_INFO};
    createInfo.applicationInfo.applicationVersion = 1;
    strcpy_s(createInfo.applicationInfo.applicationName, "VRInject");
    createInfo.applicationInfo.apiVersion = XR_API_VERSION_1_0;
    
    // Query available extensions
    uint32_t extensionCount = 0;
    xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr);
    std::vector<XrExtensionProperties> extensionProperties(extensionCount, {XR_TYPE_EXTENSION_PROPERTIES});
    xrEnumerateInstanceExtensionProperties(nullptr, extensionCount, &extensionCount, extensionProperties.data());
    
    std::vector<const char*> enabledExtensions;
    LOG_INFO("OpenXR Supported Extensions:");
    for (const auto& prop : extensionProperties) {
        LOG_INFO("  - %s", prop.extensionName);
    }
    auto isExtensionSupported = [&](const char* extName) {
        for (const auto& prop : extensionProperties) {
            if (strcmp(prop.extensionName, extName) == 0) return true;
        }
        return false;
    };
    
    if (m_api == GraphicsAPI::DX11 && isExtensionSupported(XR_KHR_D3D11_ENABLE_EXTENSION_NAME))
        enabledExtensions.push_back(XR_KHR_D3D11_ENABLE_EXTENSION_NAME);
    if (m_api == GraphicsAPI::DX12 && isExtensionSupported(XR_KHR_D3D12_ENABLE_EXTENSION_NAME))
        enabledExtensions.push_back(XR_KHR_D3D12_ENABLE_EXTENSION_NAME);
    if (m_api == GraphicsAPI::VULKAN && isExtensionSupported(XR_KHR_VULKAN_ENABLE_EXTENSION_NAME))
        enabledExtensions.push_back(XR_KHR_VULKAN_ENABLE_EXTENSION_NAME);

    // (Removed headless mode as we need graphics binding for rendering)

    createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
    createInfo.enabledExtensionNames = enabledExtensions.empty() ? nullptr : enabledExtensions.data();

    XrResult res = xrCreateInstance(&createInfo, &m_instance);
    if (XR_FAILED(res)) {
        LOG_ERROR("Failed to create OpenXR instance. (Code: %d)", res);
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
        
        XrGraphicsRequirementsD3D11KHR graphicsRequirements = {XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
        if (XR_SUCCEEDED(res) && pfnGetD3D11GraphicsRequirementsKHR) {
            res = pfnGetD3D11GraphicsRequirementsKHR(m_instance, m_systemId, &graphicsRequirements);
            if (XR_FAILED(res)) {
                LOG_ERROR("Failed to query OpenXR D3D11 graphics requirements (Res: %d)", res);
                return false;
            }
        } else {
            LOG_ERROR("Failed to resolve xrGetD3D11GraphicsRequirementsKHR function pointer.");
            return false;
        }

        m_d3dDevice = static_cast<ID3D11Device*>(nativeDevice);
        m_d3dDevice->GetImmediateContext(&m_d3dContext);

        LUID oxrLuid = graphicsRequirements.adapterLuid;
        LUID devLuid = {0,0};
        
        Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
        if (SUCCEEDED(m_d3dDevice->QueryInterface(__uuidof(IDXGIDevice), &dxgiDevice))) {
            Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
            if (SUCCEEDED(dxgiDevice->GetAdapter(&adapter))) {
                DXGI_ADAPTER_DESC desc;
                adapter->GetDesc(&desc);
                devLuid = desc.AdapterLuid;
            }
        }

        LOG_INFO("OpenXR DX11 expects Adapter LUID: %ld,%ld. Game Device LUID: %ld,%ld", 
            oxrLuid.HighPart, oxrLuid.LowPart, devLuid.HighPart, devLuid.LowPart);

        if (oxrLuid.HighPart != devLuid.HighPart || oxrLuid.LowPart != devLuid.LowPart) {
            LOG_WARN("Adapter LUID mismatch! Creating cross-adapter proxy device for OpenXR.");
            Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
            if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory4), &factory))) {
                Microsoft::WRL::ComPtr<IDXGIAdapter> targetAdapter;
                if (SUCCEEDED(factory->EnumAdapterByLuid(oxrLuid, __uuidof(IDXGIAdapter), &targetAdapter))) {
                    D3D_FEATURE_LEVEL featureLevel;
                    HRESULT hr = D3D11CreateDevice(targetAdapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &m_proxyDevice, &featureLevel, &m_proxyContext);
                    if (SUCCEEDED(hr)) {
                        LOG_INFO("Successfully created proxy D3D11 device on OpenXR adapter.");
                        graphicsBindingDX11.device = m_proxyDevice;
                    } else {
                        LOG_ERROR("Failed to create proxy D3D11 device. HR: 0x%08X", hr);
                    }
                }
            }
        } else {
            graphicsBindingDX11.device = m_d3dDevice;
        }

        if (!graphicsBindingDX11.device) {
            graphicsBindingDX11.device = m_d3dDevice;
        }

        m_depthReprojector.Initialize(m_d3dDevice);
        sessionInfo.next = &graphicsBindingDX11;
    } else if (api == GraphicsAPI::DX12) {
        PFN_xrGetD3D12GraphicsRequirementsKHR pfnGetD3D12GraphicsRequirementsKHR = nullptr;
        XrResult res = xrGetInstanceProcAddr(m_instance, "xrGetD3D12GraphicsRequirementsKHR", 
            (PFN_xrVoidFunction*)&pfnGetD3D12GraphicsRequirementsKHR);
        
        XrGraphicsRequirementsD3D12KHR graphicsRequirements = {XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR};
        if (XR_SUCCEEDED(res) && pfnGetD3D12GraphicsRequirementsKHR) {
            res = pfnGetD3D12GraphicsRequirementsKHR(m_instance, m_systemId, &graphicsRequirements);
            if (XR_FAILED(res)) {
                LOG_ERROR("Failed to query OpenXR D3D12 graphics requirements (Res: %d)", res);
                return false;
            }
        } else {
            LOG_ERROR("Failed to resolve xrGetD3D12GraphicsRequirementsKHR function pointer.");
            return false;
        }

        DX12Renderer* dx12Renderer = static_cast<DX12Renderer*>(m_renderer);
        graphicsBindingDX12.device = static_cast<ID3D12Device*>(nativeDevice);
        graphicsBindingDX12.queue = dx12Renderer ? dx12Renderer->GetVRCommandQueue() : static_cast<ID3D12CommandQueue*>(nativeQueue);
        sessionInfo.next = &graphicsBindingDX12;

        LUID oxrLuid = graphicsRequirements.adapterLuid;
        LUID devLuid = {0,0};
        if (graphicsBindingDX12.device) {
            LUID nodeLuid = graphicsBindingDX12.device->GetAdapterLuid();
            devLuid = nodeLuid;
        }
        LOG_INFO("OpenXR expects Adapter LUID: %ld,%ld. Game Device LUID: %ld,%ld", 
            oxrLuid.HighPart, oxrLuid.LowPart, devLuid.HighPart, devLuid.LowPart);
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
        LOG_ERROR("Failed to create OpenXR session. Code: %d", res);
        return false;
    }
    m_aimPredictor.Reset();
    return true;
}

bool OpenXRManager::CreateSwapchains(uint32_t width, uint32_t height, int64_t gameFormat) {
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

    m_renderWidth = width;
    m_renderHeight = height;

    uint32_t formatCount = 0;
    if (XR_FAILED(xrEnumerateSwapchainFormats(m_session, 0, &formatCount, nullptr))) return false;

    std::vector<int64_t> formats(formatCount);
    if (XR_FAILED(xrEnumerateSwapchainFormats(m_session, formatCount, &formatCount, formats.data()))) return false;

    LOG_INFO("OpenXR Supported Swapchain Formats:");
    for (int64_t f : formats) {
        LOG_INFO("  - %lld", f);
    }

    int64_t selectedFormat = -1;
    std::vector<int64_t> preferredFormats;
    // Force OpenXR to prefer non-SRGB formats. D3D12 CopyResource requires exact format matches,
    // and we cannot create UAVs for SRGB textures, so we must use UNORM and do SRGB conversion manually.
    preferredFormats.push_back(DXGI_FORMAT_R8G8B8A8_UNORM);
    preferredFormats.push_back(DXGI_FORMAT_B8G8R8A8_UNORM);
    preferredFormats.push_back(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
    preferredFormats.push_back(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);

    for (int64_t preferredFormat : preferredFormats) {
        for (int64_t runtimeFormat : formats) {
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

    if (m_api == GraphicsAPI::DX12) {
        static_cast<DX12Renderer*>(m_renderer)->SetVRFormat(selectedFormat);
    }

    for (int i = 0; i < 2; ++i) {
        XrSwapchainCreateInfo createInfo = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
        createInfo.arraySize = 1;
        createInfo.format = selectedFormat;
        createInfo.width = width;
        createInfo.height = height;
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

        if (m_api == GraphicsAPI::DX12) {
            static_cast<DX12Renderer*>(m_renderer)->SetVRResolutionAndFormat(m_swapchains[i].width, m_swapchains[i].height, selectedFormat);
            m_swapchains[i].imagesD3D12.resize(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});
            if (XR_FAILED(xrEnumerateSwapchainImages(m_swapchains[i].handle, imageCount, &imageCount, (XrSwapchainImageBaseHeader*)m_swapchains[i].imagesD3D12.data()))) return false;
        } else {
            m_swapchains[i].images.resize(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
            if (XR_FAILED(xrEnumerateSwapchainImages(m_swapchains[i].handle, imageCount, &imageCount, (XrSwapchainImageBaseHeader*)m_swapchains[i].images.data()))) return false;
        }
    }

    if (m_proxyDevice) {
        for (int i = 0; i < 2; ++i) {
            m_crossAdapterCopiers[i].Initialize(m_d3dDevice, m_proxyDevice, m_swapchains[i].width, m_swapchains[i].height, (DXGI_FORMAT)selectedFormat);
        }
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

void OpenXRManager::PollEvents() {
    XrEventDataBuffer eventData = {XR_TYPE_EVENT_DATA_BUFFER};
    while (xrPollEvent(m_instance, &eventData) == XR_SUCCESS) {
        if (eventData.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            XrEventDataSessionStateChanged* sessionStateChanged = reinterpret_cast<XrEventDataSessionStateChanged*>(&eventData);
            XrSessionState state = sessionStateChanged->state;
            
            LOG_INFO("OpenXR Session State Changed: %d", state);

            switch (state) {
                case XR_SESSION_STATE_READY: {
                    XrSessionBeginInfo sessionBeginInfo = {XR_TYPE_SESSION_BEGIN_INFO};
                    sessionBeginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    if (XR_SUCCEEDED(xrBeginSession(m_session, &sessionBeginInfo))) {
                        m_sessionRunning = true;
                        LOG_INFO("OpenXR Session Begun!");
                    } else {
                        LOG_ERROR("Failed to begin OpenXR session");
                    }
                    break;
                }
                case XR_SESSION_STATE_STOPPING: {
                    m_sessionRunning = false;
                    xrEndSession(m_session);
                    LOG_INFO("OpenXR Session Ended");
                    break;
                }
                case XR_SESSION_STATE_EXITING:
                case XR_SESSION_STATE_LOSS_PENDING: {
                    m_sessionRunning = false;
                    break;
                }
                default:
                    break;
            }
        }
        eventData = {XR_TYPE_EVENT_DATA_BUFFER};
    }
}

bool OpenXRManager::BeginFrame(XrFrameState& frameState) {
    if (!m_sessionRunning) return false;

    XrFrameWaitInfo waitInfo = {XR_TYPE_FRAME_WAIT_INFO};
    if (XR_FAILED(xrWaitFrame(m_session, &waitInfo, &frameState))) return false;
    
    m_latestPredictedDisplayTime = frameState.predictedDisplayTime;
    
    // Cache the head pose for the game engine hooks
    XrPosef latestPose;
    if (GetHeadPose(frameState.predictedDisplayTime, latestPose)) {
        m_latestHeadPose = latestPose;
    }

    // Poll VR controllers to update the XInput emulation layer
    PollActions(frameState.predictedDisplayTime);

    XrFrameBeginInfo beginInfo = {XR_TYPE_FRAME_BEGIN_INFO};
    if (XR_FAILED(xrBeginFrame(m_session, &beginInfo))) return false;

    // Spec: If xrBeginFrame succeeds, we MUST call xrEndFrame, even if shouldRender is false!
    return true; 
}

bool OpenXRManager::EndFrame(XrFrameState frameState, TextureHandle leftEye, TextureHandle rightEye, TextureHandle depthBuffer, const StereoParams* params) {
    if (!frameState.shouldRender) {
        // If we shouldn't render, just submit 0 layers
        XrFrameEndInfo endInfo = {XR_TYPE_FRAME_END_INFO};
        endInfo.displayTime = frameState.predictedDisplayTime;
        endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        endInfo.layerCount = 0;
        endInfo.layers = nullptr;
        
        XrResult endRes = xrEndFrame(m_session, &endInfo);
        
        // WE MUST SIGNAL THE VR FENCE EVEN IF NOT RENDERING TO PREVENT GAME QUEUE DEADLOCKS!
        if (m_api == GraphicsAPI::DX12) {
            static_cast<DX12Renderer*>(m_renderer)->SkipVRFrame(); 
        }
        return XR_SUCCEEDED(endRes);
    }

    XrViewLocateInfo viewLocateInfo = {XR_TYPE_VIEW_LOCATE_INFO};
    viewLocateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    viewLocateInfo.displayTime = frameState.predictedDisplayTime;
    viewLocateInfo.space = m_appSpace; // Locate relative to appSpace to avoid runtime errors

    XrViewState viewState = {XR_TYPE_VIEW_STATE};
    uint32_t viewCount = 0;
    XrView views[2] = {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
    XrResult locRes = xrLocateViews(m_session, &viewLocateInfo, &viewState, 2, &viewCount, views);
    if (XR_FAILED(locRes)) {
        LOG_ERROR("xrLocateViews failed: %d", locRes);
        return false;
    }

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

        ID3D11Texture2D* rightEyeSwapchainTex = m_swapchains[i].images[imageIndex].texture;
        
        ID3D11Texture2D* renderTargetTex = m_proxyDevice ? m_crossAdapterCopiers[i].GetGameSharedTexture() : rightEyeSwapchainTex;

        if (i == 1 && !eyeTextures[i].nativePtr && leftEye.nativePtr && depthBuffer.nativePtr && params && m_d3dDevice) {
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> colorSRV;
            m_d3dDevice->CreateShaderResourceView((ID3D11Resource*)leftEye.nativePtr, nullptr, &colorSRV);
            
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depthSRV;
            m_d3dDevice->CreateShaderResourceView((ID3D11Resource*)depthBuffer.nativePtr, nullptr, &depthSRV);

            Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> rightEyeUAV;
            m_d3dDevice->CreateUnorderedAccessView(renderTargetTex, nullptr, &rightEyeUAV);

            if (colorSRV && depthSRV && rightEyeUAV) {
                m_depthReprojector.Reproject(m_d3dContext, colorSRV.Get(), depthSRV.Get(), rightEyeUAV.Get(), *params);
            }
        } else if (m_renderer) {
            void* destTex = (m_api == GraphicsAPI::DX12) 
                ? (void*)m_swapchains[i].imagesD3D12[imageIndex].texture 
                : (void*)renderTargetTex;
            
            if (m_api == GraphicsAPI::DX12) {
                static_cast<DX12Renderer*>(m_renderer)->CopyToSwapchainVR(destTex, params);
            } else if (eyeTextures[i].nativePtr) {
                m_renderer->CopyToSwapchain(eyeTextures[i], destTex);
            }
        }

        if (m_proxyDevice && m_api == GraphicsAPI::DX11) {
            m_crossAdapterCopiers[i].CopyFrame(m_d3dContext, m_proxyContext, renderTargetTex, rightEyeSwapchainTex);
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
    layer.space = m_appSpace; // World-locked — SteamVR Motion Smoothing operates correctly
    layer.viewCount = 2;
    layer.views = projectionViews;

    const XrCompositionLayerBaseHeader* layers[1] = {reinterpret_cast<const XrCompositionLayerBaseHeader*>(&layer)};

    XrFrameEndInfo endInfo = {XR_TYPE_FRAME_END_INFO};
    endInfo.displayTime = frameState.predictedDisplayTime;
    endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    if (viewCount == 2) {
        endInfo.layerCount = 1;
        endInfo.layers = layers;
    } else {
        endInfo.layerCount = 0;
        endInfo.layers = nullptr;
    }
    
    XrResult endRes = xrEndFrame(m_session, &endInfo);
    if (XR_FAILED(endRes)) {
        LOG_ERROR("xrEndFrame failed: %d", endRes);
        return false;
    }
    return true;
}

void OpenXRManager::VRRenderLoop() {
    LOG_INFO("OpenXR VRRenderLoop thread started.");
    
    // Elevate thread to highest priority to ensure 90Hz+ VR frame pacing
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    while (m_vrThreadRunning) {
        PollEvents();
        
        if (!m_sessionRunning) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        XrFrameState frameState = {XR_TYPE_FRAME_STATE};
        if (BeginFrame(frameState)) {
            vrinject::StereoParams params;
            auto& cfg = ConfigManager::GetInstance().GetConfig();
            params.ipd = cfg.ipd;
            params.convergence = cfg.convergence;
            params.depthStrength = 1.0f;
            params.reversedZ = 1; // Hogwarts Legacy uses reversed-Z depth

            DirectX::XMFLOAT4X4 camMat;
            if (MatrixClassifier::Get().GetCameraMatrix(camMat)) {
                // Determine focal length from projection matrix M22 (1.0 / tan(fov/2))
                if (camMat._22 > 0.001f) {
                    params.focalLength = 1.0f / camMat._22;
                }
            }

            TextureHandle dummy = {};
            EndFrame(frameState, dummy, dummy, dummy, &params);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    LOG_INFO("OpenXR VRRenderLoop thread stopped.");
}

bool OpenXRManager::GetHeadPose(XrTime time, XrPosef& outPose) {
    XrSpaceLocation spaceLocation = {XR_TYPE_SPACE_LOCATION};
    if (XR_FAILED(xrLocateSpace(m_headSpace, m_appSpace, time, &spaceLocation))) return false;
    
    outPose = spaceLocation.pose;
    return (spaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0;
}

bool OpenXRManager::GetEyeFov(int eyeIndex, XrFovf& outFov) const {
    if (eyeIndex < 0 || eyeIndex > 1) return false;
    outFov = m_viewConfigs[eyeIndex].fov;
    return true;
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
    CreateAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "thumb_click_l", &m_actionThumbClickL);
    CreateAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "thumb_click_r", &m_actionThumbClickR);
    CreateAction(XR_ACTION_TYPE_FLOAT_INPUT, "trigger_l", &m_actionTriggerL);
    CreateAction(XR_ACTION_TYPE_FLOAT_INPUT, "trigger_r", &m_actionTriggerR);
    CreateAction(XR_ACTION_TYPE_FLOAT_INPUT, "grip_l", &m_actionGripL);
    CreateAction(XR_ACTION_TYPE_FLOAT_INPUT, "grip_r", &m_actionGripR);
    CreateAction(XR_ACTION_TYPE_VECTOR2F_INPUT, "thumbstick_l", &m_actionThumbstickL);
    CreateAction(XR_ACTION_TYPE_VECTOR2F_INPUT, "thumbstick_r", &m_actionThumbstickR);
    CreateAction(XR_ACTION_TYPE_VIBRATION_OUTPUT, "haptic", &m_actionHaptic);
    
    xrStringToPath(m_instance, "/user/hand/left", &m_handSubactionPath[0]);
    xrStringToPath(m_instance, "/user/hand/right", &m_handSubactionPath[1]);

    // Bindings for Oculus Touch
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
        {m_actionThumbClickL, GetPath("/user/hand/left/input/thumbstick/click")},
        {m_actionThumbClickR, GetPath("/user/hand/right/input/thumbstick/click")},
        {m_actionTriggerL, GetPath("/user/hand/left/input/trigger/value")},
        {m_actionTriggerR, GetPath("/user/hand/right/input/trigger/value")},
        {m_actionGripL, GetPath("/user/hand/left/input/squeeze/value")},
        {m_actionGripR, GetPath("/user/hand/right/input/squeeze/value")},
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

    // --- Controller aim pose tracking ---
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

    // --- HMD head pose tracking for latency compensation ---
    XrSpaceLocation headLocation = { XR_TYPE_SPACE_LOCATION };
    xrLocateSpace(m_headSpace, m_appSpace, displayTime, &headLocation);

    if ((headLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) &&
        (headLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT)) {

        XrQuaternionf headOrient = headLocation.pose.orientation;

        // Normalize to prevent quaternion drift
        float len = std::sqrt(
            headOrient.x * headOrient.x +
            headOrient.y * headOrient.y +
            headOrient.z * headOrient.z +
            headOrient.w * headOrient.w);
        if (len > 0.001f) {
            headOrient.x /= len; headOrient.y /= len;
            headOrient.z /= len; headOrient.w /= len;
        }

        // Convert OpenXR Quaternion to Euler angles (Pitch and Yaw)
        float sinp = 2.0f * (headOrient.w * headOrient.y - headOrient.z * headOrient.x);
        float pitch = std::abs(sinp) >= 1.0f ? std::copysign(3.14159265f / 2.0f, sinp) : std::asin(sinp);

        float siny_cosp = 2.0f * (headOrient.w * headOrient.z + headOrient.x * headOrient.y);
        float cosy_cosp = 1.0f - 2.0f * (headOrient.y * headOrient.y + headOrient.z * headOrient.z);
        float yaw = std::atan2(siny_cosp, cosy_cosp);

        float pitchDeg = pitch * (180.0f / 3.14159265f);
        float yawDeg = yaw * (180.0f / 3.14159265f);

        static float lastPitch = pitchDeg;
        static float lastYaw = yawDeg;
        static bool firstFrame = true;

        if (firstFrame) {
            lastPitch = pitchDeg;
            lastYaw = yawDeg;
            firstFrame = false;
        }

        // Compute pitch/yaw delta from head movement and inject as game camera rotation
        AimDelta headDelta = { pitchDeg - lastPitch, yawDeg - lastYaw };
        
        lastPitch = pitchDeg;
        lastYaw = yawDeg;

        if (std::abs(headDelta.pitchDeg) > 0.01f || std::abs(headDelta.yawDeg) > 0.01f) {
            InputHook::GetInstance().InjectAimDelta(headDelta.pitchDeg, headDelta.yawDeg);
        }
    } else {
        // Tracking lost — reset predictor to avoid stale delta on recovery
        LOG_WARN("Head tracking lost — camera injection paused");
    }

    auto GetBool = [&](XrAction action, bool* outActive = nullptr) -> bool {
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = action;
        XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
        xrGetActionStateBoolean(m_session, &getInfo, &state);
        if (outActive) *outActive = state.isActive;
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

    bool vrActive = false;
    if (GetBool(m_actionA, &vrActive)) xstate.Gamepad.wButtons |= XINPUT_GAMEPAD_A;
    if (GetBool(m_actionB)) xstate.Gamepad.wButtons |= XINPUT_GAMEPAD_B;
    if (GetBool(m_actionX)) xstate.Gamepad.wButtons |= XINPUT_GAMEPAD_X;
    if (GetBool(m_actionY)) xstate.Gamepad.wButtons |= XINPUT_GAMEPAD_Y;
    if (GetBool(m_actionMenu)) xstate.Gamepad.wButtons |= XINPUT_GAMEPAD_START;
    if (GetBool(m_actionThumbClickL)) xstate.Gamepad.wButtons |= XINPUT_GAMEPAD_LEFT_THUMB;
    if (GetBool(m_actionThumbClickR)) xstate.Gamepad.wButtons |= XINPUT_GAMEPAD_RIGHT_THUMB;
    if (GetFloat(m_actionGripL) > 0.5f) xstate.Gamepad.wButtons |= XINPUT_GAMEPAD_LEFT_SHOULDER;
    if (GetFloat(m_actionGripR) > 0.5f) xstate.Gamepad.wButtons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;

    InputHook::GetInstance().SetVRControllersActive(vrActive);

    xstate.Gamepad.bLeftTrigger = (BYTE)(GetFloat(m_actionTriggerL) * 255.0f);
    xstate.Gamepad.bRightTrigger = (BYTE)(GetFloat(m_actionTriggerR) * 255.0f);

    XrVector2f stickL = GetVec2(m_actionThumbstickL);
    XrVector2f stickR = GetVec2(m_actionThumbstickR);
    
    xstate.Gamepad.sThumbLX = (SHORT)(stickL.x * 32767.0f);
    xstate.Gamepad.sThumbLY = (SHORT)(stickL.y * 32767.0f);
    xstate.Gamepad.sThumbRX = (SHORT)(stickR.x * 32767.0f);
    xstate.Gamepad.sThumbRY = (SHORT)(stickR.y * 32767.0f);

    InputHook::GetInstance().UpdateEmulatedState(xstate);
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
