#include "sdk_session.h"

#include "sdk_validation.h"

#define XR_USE_GRAPHICS_API_D3D11
#define XR_USE_GRAPHICS_API_D3D12
#define XR_USE_PLATFORM_WIN32

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <windows.h>

#include <array>
#include <cstdlib>
#include <format>
#include <map>
#include <cstring>
#include <mutex>

namespace nexvr::sdk {
namespace {

constexpr const char* kRequiredExtension = XR_KHR_D3D11_ENABLE_EXTENSION_NAME;
constexpr const char* kRequiredExtensionD3D12 = XR_KHR_D3D12_ENABLE_EXTENSION_NAME;
constexpr XrViewConfigurationType kViewConfig = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

/** XrResult as its spec name where the loader knows it, else the number. */
std::string ResultString(XrInstance instance, XrResult result) {
    char buffer[XR_MAX_RESULT_STRING_SIZE]{};
    if (instance != XR_NULL_HANDLE && XR_SUCCEEDED(xrResultToString(instance, result, buffer))) {
        return buffer;
    }

    // Before an instance exists there is nobody to ask, and the handful of
    // results reachable at that point are the ones a reader most needs named.
    switch (result) {
        case XR_SUCCESS:                        return "XR_SUCCESS";
        case XR_ERROR_RUNTIME_UNAVAILABLE:      return "XR_ERROR_RUNTIME_UNAVAILABLE";
        case XR_ERROR_EXTENSION_NOT_PRESENT:    return "XR_ERROR_EXTENSION_NOT_PRESENT";
        case XR_ERROR_FORM_FACTOR_UNAVAILABLE:  return "XR_ERROR_FORM_FACTOR_UNAVAILABLE";
        case XR_ERROR_FORM_FACTOR_UNSUPPORTED:  return "XR_ERROR_FORM_FACTOR_UNSUPPORTED";
        case XR_ERROR_INITIALIZATION_FAILED:    return "XR_ERROR_INITIALIZATION_FAILED";
        case XR_ERROR_API_LAYER_NOT_PRESENT:    return "XR_ERROR_API_LAYER_NOT_PRESENT";
        default: break;
    }
    return std::format("XrResult({})", static_cast<int>(result));
}

double MillisecondsBetween(const LARGE_INTEGER& start, const LARGE_INTEGER& end) {
    static LARGE_INTEGER frequency = [] {
        LARGE_INTEGER value{};
        QueryPerformanceFrequency(&value);
        return value;
    }();
    if (frequency.QuadPart == 0) {
        return 0.0;
    }
    return static_cast<double>(end.QuadPart - start.QuadPart) * 1000.0 /
           static_cast<double>(frequency.QuadPart);
}

/**
 * XrView to the public pose type, field for field.
 *
 * Written out once, here, rather than at the call site. The bug this whole
 * change exists to fix was a hand-copy at a call site that omitted the poses
 * entirely, and the second-best version of that bug is a hand-copy that
 * transposes a quaternion component. One function, one place to be wrong, one
 * place a test can reach.
 *
 * The FOV order is OpenXR's own: angleLeft, angleRight, angleUp, angleDown, all
 * in radians, and left/down are negative for a normal forward-facing view. That
 * is documented on NexVR_View::fovAngles because an engine that assumes a
 * symmetric FOV and takes |angleRight| for both sides gets a subtly wrong
 * frustum on every headset that is not symmetric — which is most of them.
 */
NexVR_View ToPublicView(const XrView& view) noexcept {
    NexVR_View out{};
    out.pose.orientation[0] = view.pose.orientation.x;
    out.pose.orientation[1] = view.pose.orientation.y;
    out.pose.orientation[2] = view.pose.orientation.z;
    out.pose.orientation[3] = view.pose.orientation.w;
    out.pose.position[0] = view.pose.position.x;
    out.pose.position[1] = view.pose.position.y;
    out.pose.position[2] = view.pose.position.z;
    out.fovAngles[0] = view.fov.angleLeft;
    out.fovAngles[1] = view.fov.angleRight;
    out.fovAngles[2] = view.fov.angleUp;
    out.fovAngles[3] = view.fov.angleDown;
    return out;
}

}  // namespace

struct SdkSession::Impl {
    XrInstance instance{XR_NULL_HANDLE};
    XrSystemId systemId{XR_NULL_SYSTEM_ID};
    XrSession session{XR_NULL_HANDLE};
    XrSpace space{XR_NULL_HANDLE};
    XrSwapchain swapchain{XR_NULL_HANDLE};
    XrSwapchain depthSwapchain{XR_NULL_HANDLE};
    int32_t outstandingDepthImage{-1};
    XrSessionState state{XR_SESSION_STATE_UNKNOWN};
    XrFrameState frameState{XR_TYPE_FRAME_STATE};
    std::vector<XrViewConfigurationView> viewConfigs;
    bool exitRequested{false};

    // --- Cross-thread frame handoff ---------------------------------------
    // xrWaitFrame runs on the simulation thread and xrBeginFrame on the render
    // thread, so the frame state has to survive the gap between them. Guarded
    // by its own mutex, which protects only this map — it is never held across
    // an OpenXR or D3D call.

    /**
     * Everything the render thread needs about a frame the sim thread waited on.
     *
     * The views live here rather than being located again at submit time. The
     * game was handed these exact poses by WaitFrame and rendered from them, so
     * these are what the composition layer must name — see the note on
     * FrameHandoff::views for why a fresher pose here would be a defect and not
     * an improvement.
     */
    struct PendingFrame {
        XrFrameState frameState{XR_TYPE_FRAME_STATE};
        std::vector<XrView> views;
        bool posesValid{false};
    };

    std::mutex handoffMutex;
    std::map<uint64_t, PendingFrame> pendingFrames;
    uint64_t nextToken{1};

    /**
     * Index of a swapchain image acquired but not yet successfully waited on.
     *
     * When xrWaitSwapchainImage times out, the image stays acquired — the
     * runtime still owns our slot. Dropping the frame and acquiring a fresh
     * image next time would leak slots until acquisition itself failed, so the
     * next submission retries the wait on this one instead.
     */
    int32_t outstandingImage{-1};

    /** Half a frame at 90 Hz. Overridable so the timeout path can be forced. */
    int64_t swapchainWaitNanos{5'500'000};

    /** Fault injection: force the timeout branch every Nth frame. 0 = never. */
    uint32_t simulateTimeoutEvery{0};
    uint32_t waitCounter{0};

    /**
     * Fault injection: report every Nth frame as not worth rendering.
     *
     * The skipped-frame path is the one a game hits when the headset comes off
     * the head, and it could not be reached on the development machine at all:
     * SteamVR with no HMD attached reported shouldRender on every one of ninety
     * consecutive frames. So the branch that lets a game submit a token with no
     * texture — the entire fix for SDK-02 — had never executed end to end.
     *
     * Same reasoning as simulateTimeoutEvery, and the same shape: an error path
     * that has never run is indistinguishable from one that does not work.
     */
    uint32_t simulateSkipEvery{0};
    uint32_t skipCounter{0};

    // --- 6DOF Controller Input & Haptics ----------------------------------
    bool inputInitialized{false};
    XrActionSet actionSet{XR_NULL_HANDLE};
    XrPath handSubactionPaths[2]{XR_NULL_PATH, XR_NULL_PATH}; // 0 = left, 1 = right

    XrAction actionGripPose{XR_NULL_HANDLE};
    XrAction actionAimPose{XR_NULL_HANDLE};
    XrSpace gripSpaces[2]{XR_NULL_HANDLE, XR_NULL_HANDLE};
    XrSpace aimSpaces[2]{XR_NULL_HANDLE, XR_NULL_HANDLE};

    XrAction actionTriggerValue{XR_NULL_HANDLE};
    XrAction actionSqueezeValue{XR_NULL_HANDLE};
    XrAction actionThumbstickXY{XR_NULL_HANDLE};

    XrAction actionPrimaryClick{XR_NULL_HANDLE};
    XrAction actionSecondaryClick{XR_NULL_HANDLE};
    XrAction actionThumbstickClick{XR_NULL_HANDLE};
    XrAction actionMenuClick{XR_NULL_HANDLE};
    XrAction actionTriggerClick{XR_NULL_HANDLE};
    XrAction actionSqueezeClick{XR_NULL_HANDLE};

    XrAction actionPrimaryTouch{XR_NULL_HANDLE};
    XrAction actionSecondaryTouch{XR_NULL_HANDLE};
    XrAction actionThumbstickTouch{XR_NULL_HANDLE};
    XrAction actionTriggerTouch{XR_NULL_HANDLE};
    XrAction actionThumbrestTouch{XR_NULL_HANDLE};

    XrAction actionVibrate{XR_NULL_HANDLE};
    int64_t lastPredictedDisplayTime{0};
};

const char* StageName(Stage stage) noexcept {
    switch (stage) {
        case Stage::NotStarted:                  return "NOT_STARTED";
        case Stage::LoaderFound:                 return "LOADER_FOUND";
        case Stage::InstanceCreated:             return "INSTANCE_CREATED";
        case Stage::SystemFound:                 return "SYSTEM_FOUND";
        case Stage::GraphicsRequirementsQueried: return "GRAPHICS_REQUIREMENTS_QUERIED";
        case Stage::SessionCreated:              return "SESSION_CREATED";
        case Stage::SwapchainsCreated:           return "SWAPCHAINS_CREATED";
        case Stage::SessionRunning:              return "SESSION_RUNNING";
        case Stage::FrameSubmitted:              return "FRAME_SUBMITTED";
    }
    return "UNKNOWN";
}

SdkSession::~SdkSession() {
    Shutdown();
    delete m_impl;
    m_impl = nullptr;
}

bool SdkSession::QueryRequirements(Requirements& requirements, std::string& error) {
    if (m_impl == nullptr) {
        m_impl = new Impl{};
    }

    // --- Instance ----------------------------------------------------------
    //
    // D3D11 enable is REQUIRED and its absence is fatal — without it there is no
    // way to hand the runtime a texture. Depth is OPTIONAL and asked for only
    // when the runtime lists it, because enabling an unsupported extension makes
    // xrCreateInstance fail outright: requesting depth unconditionally would
    // turn a quality improvement into a hard incompatibility with every runtime
    // that does not implement it.
    std::vector<const char*> extensions{kRequiredExtension};

    uint32_t extensionCount = 0;
    xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr);
    std::vector<XrExtensionProperties> available(extensionCount,
                                                 XrExtensionProperties{XR_TYPE_EXTENSION_PROPERTIES});
    if (extensionCount > 0) {
        xrEnumerateInstanceExtensionProperties(nullptr, extensionCount, &extensionCount,
                                               available.data());
    }

    for (const XrExtensionProperties& property : available) {
        if (std::strcmp(property.extensionName, XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME) == 0) {
            extensions.push_back(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME);
            m_depthExtensionAvailable = true;
            break;
        }
    }

    XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.enabledExtensionNames = extensions.data();
    // OpenXR rejects an unterminated or overlong name outright, and the field is
    // a fixed array rather than a pointer, so this is a copy not an assignment.
    std::snprintf(createInfo.applicationInfo.applicationName, XR_MAX_APPLICATION_NAME_SIZE,
                  "NexVR B2B DX11 Prototype");
    createInfo.applicationInfo.applicationVersion = 1;
    std::snprintf(createInfo.applicationInfo.engineName, XR_MAX_ENGINE_NAME_SIZE, "NexVR");
    createInfo.applicationInfo.engineVersion = 1;
    createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

    XrResult result = xrCreateInstance(&createInfo, &m_impl->instance);
    m_calls.push_back({"xrCreateInstance", ResultString(XR_NULL_HANDLE, result), XR_SUCCEEDED(result)});
    if (XR_FAILED(result)) {
        error = std::format("xrCreateInstance: {}", ResultString(XR_NULL_HANDLE, result));
        return false;
    }
    m_stage = Stage::InstanceCreated;

    XrInstanceProperties properties{XR_TYPE_INSTANCE_PROPERTIES};
    if (XR_SUCCEEDED(xrGetInstanceProperties(m_impl->instance, &properties))) {
        m_runtimeName = std::format("{} {}.{}.{}", properties.runtimeName,
                                    XR_VERSION_MAJOR(properties.runtimeVersion),
                                    XR_VERSION_MINOR(properties.runtimeVersion),
                                    XR_VERSION_PATCH(properties.runtimeVersion));
    }

    // --- System ------------------------------------------------------------
    // Predicted to be where a machine with no headset stops, and MEASURED not
    // to be. SteamVR/OpenXR 2.16.7 returns XR_SUCCESS here with no HMD attached
    // and goes on to create a session, a swapchain, and accept submitted frames.
    //
    // Recorded because the prediction was load-bearing: "no headset means
    // xrGetSystem fails" is the assumption behind treating this call as a
    // hardware-presence check, and on at least one shipping runtime it is not
    // one. A system id is not evidence that a display exists.
    XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

    result = xrGetSystem(m_impl->instance, &systemInfo, &m_impl->systemId);
    m_calls.push_back({"xrGetSystem", ResultString(m_impl->instance, result), XR_SUCCEEDED(result)});
    if (XR_FAILED(result)) {
        error = std::format("xrGetSystem: {}", ResultString(m_impl->instance, result));
        return false;
    }
    m_stage = Stage::SystemFound;

    // --- Graphics requirements --------------------------------------------
    // Mandatory before xrCreateSession per the XR_KHR_D3D11_enable spec, and
    // the source of the adapter constraint documented in the header.
    auto getRequirements = PFN_xrGetD3D11GraphicsRequirementsKHR{nullptr};
    result = xrGetInstanceProcAddr(m_impl->instance, "xrGetD3D11GraphicsRequirementsKHR",
                                   reinterpret_cast<PFN_xrVoidFunction*>(&getRequirements));
    m_calls.push_back({"xrGetInstanceProcAddr(xrGetD3D11GraphicsRequirementsKHR)",
                       ResultString(m_impl->instance, result), XR_SUCCEEDED(result)});
    if (XR_FAILED(result) || getRequirements == nullptr) {
        error = "xrGetD3D11GraphicsRequirementsKHR is unavailable despite the extension being enabled";
        return false;
    }

    XrGraphicsRequirementsD3D11KHR graphics{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
    result = getRequirements(m_impl->instance, m_impl->systemId, &graphics);
    m_calls.push_back({"xrGetD3D11GraphicsRequirementsKHR", ResultString(m_impl->instance, result),
                       XR_SUCCEEDED(result)});
    if (XR_FAILED(result)) {
        error = std::format("xrGetD3D11GraphicsRequirementsKHR: {}",
                            ResultString(m_impl->instance, result));
        return false;
    }

    requirements.adapterLuid = graphics.adapterLuid;

    // --- View configuration, BEFORE any session or device -------------------
    // Deliberately here rather than in Initialize. These dimensions are what the
    // game must render at, and the game creates its render targets long before
    // it would ever call into a VR SDK. Discovering the size after the device
    // exists is how the first run of this prototype ended up copying a
    // 1024x1024 target into a 3496x1944 swapchain thirty times in a row.
    uint32_t viewCount = 0;
    result = xrEnumerateViewConfigurationViews(m_impl->instance, m_impl->systemId, kViewConfig, 0,
                                               &viewCount, nullptr);
    if (XR_FAILED(result) || viewCount == 0) {
        m_calls.push_back({"xrEnumerateViewConfigurationViews",
                           ResultString(m_impl->instance, result), false});
        error = "The runtime reported no stereo views";
        return false;
    }

    m_impl->viewConfigs.assign(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    result = xrEnumerateViewConfigurationViews(m_impl->instance, m_impl->systemId, kViewConfig,
                                               viewCount, &viewCount, m_impl->viewConfigs.data());
    m_calls.push_back({"xrEnumerateViewConfigurationViews", ResultString(m_impl->instance, result),
                       XR_SUCCEEDED(result)});
    if (XR_FAILED(result)) {
        error = std::format("xrEnumerateViewConfigurationViews: {}",
                            ResultString(m_impl->instance, result));
        return false;
    }

    m_viewCount = viewCount;
    requirements.viewCount = viewCount;
    requirements.eyeWidth = m_impl->viewConfigs.front().recommendedImageRectWidth;
    requirements.eyeHeight = m_impl->viewConfigs.front().recommendedImageRectHeight;

    m_stage = Stage::GraphicsRequirementsQueried;
    return true;
}

bool SdkSession::QueryRequirementsDX12(Requirements& requirements, std::string& error) {
    m_api = GraphicsApi::D3D12;
    if (m_impl == nullptr) {
        m_impl = new Impl{};
    }

    std::vector<const char*> extensions{kRequiredExtensionD3D12};

    uint32_t extensionCount = 0;
    xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr);
    std::vector<XrExtensionProperties> available(extensionCount,
                                                 XrExtensionProperties{XR_TYPE_EXTENSION_PROPERTIES});
    if (extensionCount > 0) {
        xrEnumerateInstanceExtensionProperties(nullptr, extensionCount, &extensionCount,
                                               available.data());
    }

    for (const XrExtensionProperties& property : available) {
        if (std::strcmp(property.extensionName, XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME) == 0) {
            extensions.push_back(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME);
            m_depthExtensionAvailable = true;
            break;
        }
    }

    XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.enabledExtensionNames = extensions.data();
    std::snprintf(createInfo.applicationInfo.applicationName, XR_MAX_APPLICATION_NAME_SIZE,
                  "NexVR B2B DX12");
    createInfo.applicationInfo.applicationVersion = 1;
    std::snprintf(createInfo.applicationInfo.engineName, XR_MAX_ENGINE_NAME_SIZE, "NexVR");
    createInfo.applicationInfo.engineVersion = 1;
    createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

    XrResult result = xrCreateInstance(&createInfo, &m_impl->instance);
    m_calls.push_back({"xrCreateInstance(D3D12)", ResultString(XR_NULL_HANDLE, result), XR_SUCCEEDED(result)});
    if (XR_FAILED(result)) {
        error = std::format("xrCreateInstance(D3D12): {}", ResultString(XR_NULL_HANDLE, result));
        return false;
    }
    m_stage = Stage::InstanceCreated;

    XrInstanceProperties properties{XR_TYPE_INSTANCE_PROPERTIES};
    if (XR_SUCCEEDED(xrGetInstanceProperties(m_impl->instance, &properties))) {
        m_runtimeName = std::format("{} {}.{}.{}", properties.runtimeName,
                                    XR_VERSION_MAJOR(properties.runtimeVersion),
                                    XR_VERSION_MINOR(properties.runtimeVersion),
                                    XR_VERSION_PATCH(properties.runtimeVersion));
    }

    XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

    result = xrGetSystem(m_impl->instance, &systemInfo, &m_impl->systemId);
    m_calls.push_back({"xrGetSystem", ResultString(m_impl->instance, result), XR_SUCCEEDED(result)});
    if (XR_FAILED(result)) {
        error = std::format("xrGetSystem: {}", ResultString(m_impl->instance, result));
        return false;
    }
    m_stage = Stage::SystemFound;

    auto getRequirements = PFN_xrGetD3D12GraphicsRequirementsKHR{nullptr};
    result = xrGetInstanceProcAddr(m_impl->instance, "xrGetD3D12GraphicsRequirementsKHR",
                                   reinterpret_cast<PFN_xrVoidFunction*>(&getRequirements));
    m_calls.push_back({"xrGetInstanceProcAddr(xrGetD3D12GraphicsRequirementsKHR)",
                       ResultString(m_impl->instance, result), XR_SUCCEEDED(result)});
    if (XR_FAILED(result) || getRequirements == nullptr) {
        error = "xrGetD3D12GraphicsRequirementsKHR is unavailable despite the extension being enabled";
        return false;
    }

    XrGraphicsRequirementsD3D12KHR graphics{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR};
    result = getRequirements(m_impl->instance, m_impl->systemId, &graphics);
    m_calls.push_back({"xrGetD3D12GraphicsRequirementsKHR", ResultString(m_impl->instance, result),
                       XR_SUCCEEDED(result)});
    if (XR_FAILED(result)) {
        error = std::format("xrGetD3D12GraphicsRequirementsKHR: {}",
                            ResultString(m_impl->instance, result));
        return false;
    }

    requirements.adapterLuid = graphics.adapterLuid;

    uint32_t viewCount = 0;
    result = xrEnumerateViewConfigurationViews(m_impl->instance, m_impl->systemId, kViewConfig, 0,
                                               &viewCount, nullptr);
    if (XR_FAILED(result) || viewCount == 0) {
        m_calls.push_back({"xrEnumerateViewConfigurationViews",
                           ResultString(m_impl->instance, result), false});
        error = "The runtime reported no stereo views";
        return false;
    }

    m_impl->viewConfigs.assign(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    result = xrEnumerateViewConfigurationViews(m_impl->instance, m_impl->systemId, kViewConfig,
                                               viewCount, &viewCount, m_impl->viewConfigs.data());
    m_calls.push_back({"xrEnumerateViewConfigurationViews", ResultString(m_impl->instance, result),
                       XR_SUCCEEDED(result)});
    if (XR_FAILED(result)) {
        error = std::format("xrEnumerateViewConfigurationViews: {}",
                            ResultString(m_impl->instance, result));
        return false;
    }

    m_viewCount = viewCount;
    requirements.viewCount = viewCount;
    requirements.eyeWidth = m_impl->viewConfigs.front().recommendedImageRectWidth;
    requirements.eyeHeight = m_impl->viewConfigs.front().recommendedImageRectHeight;

    m_stage = Stage::GraphicsRequirementsQueried;
    return true;
}

bool SdkSession::BindDevice(ID3D11Device* gameDevice, ID3D11DeviceContext* gameContext,
                            std::string& error) {
    if (gameDevice == nullptr || gameContext == nullptr) {
        error = "BindDevice was given a null device or context";
        return false;
    }
    m_device = gameDevice;

    // The CALLER'S context, kept exactly as given.
    //
    // An earlier version called GetImmediateContext here and used that instead.
    // It worked — and it silently repaired a caller who had passed a deferred
    // context, which meant NEXVR_ERROR_NOT_IMMEDIATE_CONTEXT could never fire
    // and a studio's mistake stayed invisible until something else broke.
    // Substituting a correct value for an incorrect argument is not a
    // kindness; it is a validation layer lying about what it received.
    m_context = gameContext;

    if (SUCCEEDED(m_context.As(&m_multithread))) {
        m_multithreadWasOn = m_multithread->GetMultithreadProtected() != FALSE;
        m_multithread->SetMultithreadProtected(TRUE);
        m_multithreadProtected = true;
    } else {
        m_multithread.Reset();
        m_multithreadProtected = false;
    }

    // --- Fault injection, opt-in through the environment --------------------
    // NEXVR_SDK_FAULT_INJECT_SWAPCHAIN_TIMEOUT=N makes every Nth swapchain wait
    // behave as though the compositor had not freed an image in time.
    //
    // An environment variable rather than an ABI function, deliberately. This
    // is a diagnostic hook, not a capability a studio should reach from game
    // code — and widening the C ABI to expose it would make it part of a
    // contract we then have to keep forever.
    //
    // It exists because the recovery path is otherwise unreachable. Phase 0.9
    // set the swapchain deadline to 1 ns and still measured ZERO timeouts:
    // with no headset the compositor never holds an image, so the wait always
    // returns immediately. An error path that has never executed is
    // indistinguishable from one that does not work.
    // _dupenv_s rather than getenv: MSVC deprecates the latter, and suppressing
    // that warning in a shipping library to save four lines is the wrong trade.
    char* injected = nullptr;
    size_t injectedLength = 0;
    if (_dupenv_s(&injected, &injectedLength, "NEXVR_SDK_FAULT_INJECT_SWAPCHAIN_TIMEOUT") == 0 &&
        injected != nullptr) {
        const int every = std::atoi(injected);
        if (every > 0) {
            SetSimulatedTimeoutEvery(static_cast<uint32_t>(every));
        }
        std::free(injected);
    }

    char* skipInjected = nullptr;
    size_t skipLength = 0;
    if (_dupenv_s(&skipInjected, &skipLength, "NEXVR_SDK_FAULT_INJECT_FRAME_SKIP") == 0 &&
        skipInjected != nullptr) {
        const int every = std::atoi(skipInjected);
        if (every > 0) {
            SetSimulatedSkipEvery(static_cast<uint32_t>(every));
        }
        std::free(skipInjected);
    }

    return true;
}

bool SdkSession::ContextIsImmediate() const noexcept {
    return m_context && m_context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE;
}

bool SdkSession::DeviceAdapterLuid(LUID& luid) const noexcept {
    if (!m_device) {
        return false;
    }
    ComPtr<IDXGIDevice> dxgiDevice;
    if (FAILED(m_device.As(&dxgiDevice))) {
        return false;
    }
    ComPtr<IDXGIAdapter> adapter;
    if (FAILED(dxgiDevice->GetAdapter(&adapter))) {
        return false;
    }
    DXGI_ADAPTER_DESC desc{};
    if (FAILED(adapter->GetDesc(&desc))) {
        return false;
    }
    luid = desc.AdapterLuid;
    return true;
}

bool SdkSession::BindDeviceDX12(ID3D12Device* gameDevice, ID3D12CommandQueue* gameQueue,
                                std::string& error) {
    if (gameDevice == nullptr || gameQueue == nullptr) {
        error = "BindDeviceDX12 was given a null device or queue";
        return false;
    }
    m_api = GraphicsApi::D3D12;
    m_d3d12Device = gameDevice;
    m_d3d12Queue = gameQueue;

    HRESULT hr = m_d3d12Device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_d3d12CommandAllocator));
    if (FAILED(hr)) {
        error = std::format("Failed to create D3D12 command allocator (HRESULT 0x{:08X})",
                            static_cast<uint32_t>(hr));
        return false;
    }

    hr = m_d3d12Device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_d3d12CommandAllocator.Get(), nullptr,
        IID_PPV_ARGS(&m_d3d12CommandList));
    if (FAILED(hr)) {
        error = std::format("Failed to create D3D12 command list (HRESULT 0x{:08X})",
                            static_cast<uint32_t>(hr));
        return false;
    }
    m_d3d12CommandList->Close();

    hr = m_d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_d3d12Fence));
    if (FAILED(hr)) {
        error = std::format("Failed to create D3D12 fence (HRESULT 0x{:08X})",
                            static_cast<uint32_t>(hr));
        return false;
    }
    m_d3d12FenceValue = 0;
    m_d3d12FenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (m_d3d12FenceEvent == nullptr) {
        error = "Failed to create D3D12 fence event";
        return false;
    }

    char* injected = nullptr;
    size_t injectedLength = 0;
    if (_dupenv_s(&injected, &injectedLength, "NEXVR_SDK_FAULT_INJECT_SWAPCHAIN_TIMEOUT") == 0 &&
        injected != nullptr) {
        const int every = std::atoi(injected);
        if (every > 0) {
            SetSimulatedTimeoutEvery(static_cast<uint32_t>(every));
        }
        std::free(injected);
    }

    char* skipInjected = nullptr;
    size_t skipLength = 0;
    if (_dupenv_s(&skipInjected, &skipLength, "NEXVR_SDK_FAULT_INJECT_FRAME_SKIP") == 0 &&
        skipInjected != nullptr) {
        const int every = std::atoi(skipInjected);
        if (every > 0) {
            SetSimulatedSkipEvery(static_cast<uint32_t>(every));
        }
        std::free(skipInjected);
    }

    return true;
}

bool SdkSession::QueueIsDirect() const noexcept {
    if (!m_d3d12Queue) {
        return false;
    }
    return m_d3d12Queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT;
}

bool SdkSession::DeviceAdapterLuidDX12(LUID& luid) const noexcept {
    if (!m_d3d12Device) {
        return false;
    }
    luid = m_d3d12Device->GetAdapterLuid();
    return true;
}

bool SdkSession::Initialize(ID3D11Device* gameDevice, DXGI_FORMAT targetFormat, std::string& error) {
    if (gameDevice == nullptr) {
        error = "Initialize was given a null device";
        return false;
    }
    if (m_impl == nullptr || m_impl->systemId == XR_NULL_SYSTEM_ID) {
        error = "QueryRequirements must succeed before Initialize";
        return false;
    }

    // BindDevice already took the reference, the immediate context, and the
    // ID3D11Multithread lock. Doing it again here would double-apply the
    // multithread flag and hide whether the engine had it on beforehand.
    if (!m_device) {
        error = "BindDevice must succeed before Initialize";
        return false;
    }

    // --- Session -----------------------------------------------------------
    XrGraphicsBindingD3D11KHR binding{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
    binding.device = gameDevice;

    XrSessionCreateInfo sessionInfo{XR_TYPE_SESSION_CREATE_INFO};
    sessionInfo.next = &binding;
    sessionInfo.systemId = m_impl->systemId;

    XrResult result = xrCreateSession(m_impl->instance, &sessionInfo, &m_impl->session);
    m_calls.push_back({"xrCreateSession", ResultString(m_impl->instance, result), XR_SUCCEEDED(result)});
    if (XR_FAILED(result)) {
        error = std::format("xrCreateSession: {}", ResultString(m_impl->instance, result));
        return false;
    }
    m_stage = Stage::SessionCreated;

    // --- Reference space ---------------------------------------------------
    XrReferenceSpaceCreateInfo spaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    spaceInfo.poseInReferenceSpace.orientation.w = 1.0f;

    result = xrCreateReferenceSpace(m_impl->session, &spaceInfo, &m_impl->space);
    m_calls.push_back({"xrCreateReferenceSpace", ResultString(m_impl->instance, result),
                       XR_SUCCEEDED(result)});
    if (XR_FAILED(result)) {
        error = std::format("xrCreateReferenceSpace: {}", ResultString(m_impl->instance, result));
        return false;
    }

    // View configuration was already enumerated in QueryRequirements, because
    // the game needed those dimensions before it created the device this
    // function was just handed.
    const uint32_t viewCount = m_viewCount;

    // --- Swapchain format --------------------------------------------------
    // Chosen from what the runtime offers, never assumed. A format the runtime
    // did not list is rejected at xrCreateSwapchain, and the list differs
    // between runtimes.
    uint32_t formatCount = 0;
    xrEnumerateSwapchainFormats(m_impl->session, 0, &formatCount, nullptr);
    std::vector<int64_t> formats(formatCount);
    result = xrEnumerateSwapchainFormats(m_impl->session, formatCount, &formatCount, formats.data());
    m_calls.push_back({"xrEnumerateSwapchainFormats", ResultString(m_impl->instance, result),
                       XR_SUCCEEDED(result)});
    if (XR_FAILED(result) || formats.empty()) {
        error = "The runtime offered no swapchain formats";
        return false;
    }

    // Chosen to be COPYABLE INTO, not chosen by preference and then imposed.
    //
    // The prototype picked SRGB and hoped the game matched. That gets the
    // priority backwards: the game's render target already exists and its
    // format is not ours to change, so the swapchain is what should bend. Only
    // if nothing the runtime offers shares a typeless parent with the target is
    // this genuinely unfixable — and then it is worth its own error code.
    //
    // Among compatible candidates, SRGB still wins: it is what every runtime
    // expects for a colour buffer, and staying inside the family means the copy
    // is still a straight CopyResource.
    m_format = 0;
    bool foundCompatible = false;
    for (const int64_t candidate : formats) {
        const auto asDxgi = static_cast<DXGI_FORMAT>(candidate);
        if (!CopyCompatible(targetFormat, asDxgi)) {
            continue;
        }
        if (!foundCompatible) {
            m_format = candidate;
            foundCompatible = true;
        }
        if (asDxgi == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || asDxgi == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) {
            m_format = candidate;
            break;
        }
    }

    if (!foundCompatible) {
        m_failedOnFormat = true;
        error = std::format(
            "The runtime offers {} swapchain format(s) and none can be copied into from the game's "
            "render target format {}. CopyResource requires a shared typeless parent.",
            formats.size(), static_cast<int>(targetFormat));
        return false;
    }

    // --- Swapchain ---------------------------------------------------------
    // One swapchain, double width, both eyes side by side. Two swapchains would
    // be more conventional; one keeps this prototype's copy a single
    // CopyResource so that section 5's timing measures the copy rather than the
    // loop around it.
    const XrViewConfigurationView& view = m_impl->viewConfigs.front();
    m_width = view.recommendedImageRectWidth * viewCount;
    m_height = view.recommendedImageRectHeight;

    XrSwapchainCreateInfo swapchainInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    swapchainInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT |
                               XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
    swapchainInfo.format = m_format;
    swapchainInfo.sampleCount = 1;
    swapchainInfo.width = m_width;
    swapchainInfo.height = m_height;
    swapchainInfo.faceCount = 1;
    swapchainInfo.arraySize = 1;
    swapchainInfo.mipCount = 1;

    result = xrCreateSwapchain(m_impl->session, &swapchainInfo, &m_impl->swapchain);
    m_calls.push_back({"xrCreateSwapchain", ResultString(m_impl->instance, result), XR_SUCCEEDED(result)});
    if (XR_FAILED(result)) {
        error = std::format("xrCreateSwapchain: {}", ResultString(m_impl->instance, result));
        return false;
    }

    // --- Depth swapchain, when the runtime will take one --------------------
    //
    // Optional throughout. Every failure below leaves m_depthUsable false and
    // the colour path completely untouched, because depth is a quality
    // improvement and must never become a reason a session does not start.
    //
    // The format is chosen the same way the colour format was: from what the
    // runtime OFFERS, preferring one the game's buffer can be copied into. A
    // depth format the game cannot copy into is worse than no depth at all —
    // it would create a swapchain, acquire images every frame, and submit
    // garbage the compositor would reproject against.
    if (m_depthExtensionAvailable) {
        int64_t depthFormat = 0;
        for (const int64_t candidate : formats) {
            const auto asDxgi = static_cast<DXGI_FORMAT>(candidate);
            if (!IsDepthFormat(asDxgi)) {
                continue;
            }
            if (depthFormat == 0) {
                depthFormat = candidate;
            }
            // D32_FLOAT preferred where offered: it is what a reverse-Z game
            // almost always renders into, so it is the one most likely to be a
            // straight CopyResource rather than a format-family mismatch.
            if (asDxgi == DXGI_FORMAT_D32_FLOAT) {
                depthFormat = candidate;
                break;
            }
        }

        if (depthFormat != 0) {
            XrSwapchainCreateInfo depthInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
            depthInfo.usageFlags = XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                   XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
            depthInfo.format = depthFormat;
            depthInfo.sampleCount = 1;
            depthInfo.width = m_width;
            depthInfo.height = m_height;
            depthInfo.faceCount = 1;
            depthInfo.arraySize = 1;
            depthInfo.mipCount = 1;

            const XrResult depthResult =
                xrCreateSwapchain(m_impl->session, &depthInfo, &m_impl->depthSwapchain);
            m_calls.push_back({"xrCreateSwapchain(depth)",
                               ResultString(m_impl->instance, depthResult),
                               XR_SUCCEEDED(depthResult)});

            if (XR_SUCCEEDED(depthResult)) {
                uint32_t depthImageCount = 0;
                xrEnumerateSwapchainImages(m_impl->depthSwapchain, 0, &depthImageCount, nullptr);
                std::vector<XrSwapchainImageD3D11KHR> depthImages(
                    depthImageCount, {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
                if (XR_SUCCEEDED(xrEnumerateSwapchainImages(
                        m_impl->depthSwapchain, depthImageCount, &depthImageCount,
                        reinterpret_cast<XrSwapchainImageBaseHeader*>(depthImages.data())))) {
                    for (const XrSwapchainImageD3D11KHR& image : depthImages) {
                        m_depthSwapchainImages.push_back(image.texture);
                    }
                    m_depthFormat = depthFormat;
                    m_depthUsable = !m_depthSwapchainImages.empty();
                }
            }
        }
    }

    uint32_t imageCount = 0;
    xrEnumerateSwapchainImages(m_impl->swapchain, 0, &imageCount, nullptr);
    std::vector<XrSwapchainImageD3D11KHR> images(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
    result = xrEnumerateSwapchainImages(m_impl->swapchain, imageCount, &imageCount,
                                        reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data()));
    m_calls.push_back({"xrEnumerateSwapchainImages", ResultString(m_impl->instance, result),
                       XR_SUCCEEDED(result)});
    if (XR_FAILED(result)) {
        error = std::format("xrEnumerateSwapchainImages: {}", ResultString(m_impl->instance, result));
        return false;
    }

    // The runtime created these ID3D11Texture2Ds on the game's device and owns
    // them. They are borrowed for the lifetime of the swapchain and must not be
    // released here — hence raw pointers rather than ComPtr, which would take a
    // reference this side has no business holding.
    m_swapchainImages.clear();
    for (const XrSwapchainImageD3D11KHR& image : images) {
        m_swapchainImages.push_back(image.texture);
    }

    std::string inputError;
    if (!InitializeInput(inputError)) {
        m_calls.push_back({"InitializeInput", inputError, false});
    } else {
        m_calls.push_back({"InitializeInput", "XR_SUCCESS", true});
    }

    m_stage = Stage::SwapchainsCreated;
    return true;
}

bool SdkSession::InitializeDX12(ID3D12Device* gameDevice, ID3D12CommandQueue* gameQueue,
                                DXGI_FORMAT targetFormat, std::string& error) {
    if (gameDevice == nullptr || gameQueue == nullptr) {
        error = "InitializeDX12 was given a null device or queue";
        return false;
    }
    if (m_impl == nullptr || m_impl->systemId == XR_NULL_SYSTEM_ID) {
        error = "QueryRequirementsDX12 must succeed before InitializeDX12";
        return false;
    }
    if (!m_d3d12Device || !m_d3d12Queue) {
        error = "BindDeviceDX12 must succeed before InitializeDX12";
        return false;
    }

    XrGraphicsBindingD3D12KHR binding{XR_TYPE_GRAPHICS_BINDING_D3D12_KHR};
    binding.device = gameDevice;
    binding.queue = gameQueue;

    XrSessionCreateInfo sessionInfo{XR_TYPE_SESSION_CREATE_INFO};
    sessionInfo.next = &binding;
    sessionInfo.systemId = m_impl->systemId;

    XrResult result = xrCreateSession(m_impl->instance, &sessionInfo, &m_impl->session);
    m_calls.push_back({"xrCreateSession(D3D12)", ResultString(m_impl->instance, result), XR_SUCCEEDED(result)});
    if (XR_FAILED(result)) {
        error = std::format("xrCreateSession(D3D12): {}", ResultString(m_impl->instance, result));
        return false;
    }
    m_stage = Stage::SessionCreated;

    XrReferenceSpaceCreateInfo spaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    spaceInfo.poseInReferenceSpace.orientation.w = 1.0f;

    result = xrCreateReferenceSpace(m_impl->session, &spaceInfo, &m_impl->space);
    m_calls.push_back({"xrCreateReferenceSpace", ResultString(m_impl->instance, result),
                       XR_SUCCEEDED(result)});
    if (XR_FAILED(result)) {
        error = std::format("xrCreateReferenceSpace: {}", ResultString(m_impl->instance, result));
        return false;
    }

    const uint32_t viewCount = m_viewCount;

    uint32_t formatCount = 0;
    xrEnumerateSwapchainFormats(m_impl->session, 0, &formatCount, nullptr);
    std::vector<int64_t> formats(formatCount);
    result = xrEnumerateSwapchainFormats(m_impl->session, formatCount, &formatCount, formats.data());
    m_calls.push_back({"xrEnumerateSwapchainFormats", ResultString(m_impl->instance, result),
                       XR_SUCCEEDED(result)});
    if (XR_FAILED(result) || formats.empty()) {
        error = "The runtime offered no swapchain formats";
        return false;
    }

    m_format = 0;
    bool foundCompatible = false;
    for (const int64_t candidate : formats) {
        const auto asDxgi = static_cast<DXGI_FORMAT>(candidate);
        if (!CopyCompatible(targetFormat, asDxgi)) {
            continue;
        }
        if (!foundCompatible) {
            m_format = candidate;
            foundCompatible = true;
        }
        if (asDxgi == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || asDxgi == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) {
            m_format = candidate;
            break;
        }
    }

    if (!foundCompatible) {
        m_failedOnFormat = true;
        error = std::format(
            "The runtime offers {} swapchain format(s) and none can be copied into from the game's "
            "render target format {}. CopyResource requires a shared typeless parent.",
            formats.size(), static_cast<int>(targetFormat));
        return false;
    }

    const XrViewConfigurationView& view = m_impl->viewConfigs.front();
    m_width = view.recommendedImageRectWidth * viewCount;
    m_height = view.recommendedImageRectHeight;

    XrSwapchainCreateInfo swapchainInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    swapchainInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT |
                               XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
    swapchainInfo.format = m_format;
    swapchainInfo.sampleCount = 1;
    swapchainInfo.width = m_width;
    swapchainInfo.height = m_height;
    swapchainInfo.faceCount = 1;
    swapchainInfo.arraySize = 1;
    swapchainInfo.mipCount = 1;

    result = xrCreateSwapchain(m_impl->session, &swapchainInfo, &m_impl->swapchain);
    m_calls.push_back({"xrCreateSwapchain", ResultString(m_impl->instance, result), XR_SUCCEEDED(result)});
    if (XR_FAILED(result)) {
        error = std::format("xrCreateSwapchain: {}", ResultString(m_impl->instance, result));
        return false;
    }

    if (m_depthExtensionAvailable) {
        int64_t depthFormat = 0;
        for (const int64_t candidate : formats) {
            const auto asDxgi = static_cast<DXGI_FORMAT>(candidate);
            if (!IsDepthFormat(asDxgi)) {
                continue;
            }
            if (depthFormat == 0) {
                depthFormat = candidate;
            }
            if (asDxgi == DXGI_FORMAT_D32_FLOAT) {
                depthFormat = candidate;
                break;
            }
        }

        if (depthFormat != 0) {
            XrSwapchainCreateInfo depthInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
            depthInfo.usageFlags = XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                   XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
            depthInfo.format = depthFormat;
            depthInfo.sampleCount = 1;
            depthInfo.width = m_width;
            depthInfo.height = m_height;
            depthInfo.faceCount = 1;
            depthInfo.arraySize = 1;
            depthInfo.mipCount = 1;

            const XrResult depthResult =
                xrCreateSwapchain(m_impl->session, &depthInfo, &m_impl->depthSwapchain);
            m_calls.push_back({"xrCreateSwapchain(depth)",
                               ResultString(m_impl->instance, depthResult),
                               XR_SUCCEEDED(depthResult)});

            if (XR_SUCCEEDED(depthResult)) {
                uint32_t depthImageCount = 0;
                xrEnumerateSwapchainImages(m_impl->depthSwapchain, 0, &depthImageCount, nullptr);
                std::vector<XrSwapchainImageD3D12KHR> depthImages(
                    depthImageCount, {XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});
                if (XR_SUCCEEDED(xrEnumerateSwapchainImages(
                        m_impl->depthSwapchain, depthImageCount, &depthImageCount,
                        reinterpret_cast<XrSwapchainImageBaseHeader*>(depthImages.data())))) {
                    m_d3d12DepthSwapchainImages.clear();
                    for (const XrSwapchainImageD3D12KHR& image : depthImages) {
                        m_d3d12DepthSwapchainImages.push_back(image.texture);
                    }
                    m_depthFormat = depthFormat;
                    m_depthUsable = !m_d3d12DepthSwapchainImages.empty();
                }
            }
        }
    }

    uint32_t imageCount = 0;
    xrEnumerateSwapchainImages(m_impl->swapchain, 0, &imageCount, nullptr);
    std::vector<XrSwapchainImageD3D12KHR> images(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});
    result = xrEnumerateSwapchainImages(m_impl->swapchain, imageCount, &imageCount,
                                        reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data()));
    m_calls.push_back({"xrEnumerateSwapchainImages", ResultString(m_impl->instance, result),
                       XR_SUCCEEDED(result)});
    if (XR_FAILED(result)) {
        error = std::format("xrEnumerateSwapchainImages: {}", ResultString(m_impl->instance, result));
        return false;
    }

    m_d3d12SwapchainImages.clear();
    for (const XrSwapchainImageD3D12KHR& image : images) {
        m_d3d12SwapchainImages.push_back(image.texture);
    }

    std::string inputError;
    if (!InitializeInput(inputError)) {
        m_calls.push_back({"InitializeInput", inputError, false});
    } else {
        m_calls.push_back({"InitializeInput", "XR_SUCCESS", true});
    }

    m_stage = Stage::SwapchainsCreated;
    return true;
}

bool SdkSession::PumpEvents(std::string& error) {
    if (m_impl == nullptr) {
        error = "PumpEvents before initialisation";
        return false;
    }

    for (;;) {
        XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
        const XrResult result = xrPollEvent(m_impl->instance, &event);
        if (result == XR_EVENT_UNAVAILABLE) {
            break;
        }
        if (XR_FAILED(result)) {
            error = std::format("xrPollEvent: {}", ResultString(m_impl->instance, result));
            return false;
        }

        switch (event.type) {
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                const auto& changed = reinterpret_cast<const XrEventDataSessionStateChanged&>(event);
                m_impl->state = changed.state;

                if (changed.state == XR_SESSION_STATE_READY) {
                    XrSessionBeginInfo beginInfo{XR_TYPE_SESSION_BEGIN_INFO};
                    beginInfo.primaryViewConfigurationType = kViewConfig;
                    const XrResult begun = xrBeginSession(m_impl->session, &beginInfo);
                    m_calls.push_back({"xrBeginSession", ResultString(m_impl->instance, begun),
                                       XR_SUCCEEDED(begun)});
                    if (XR_FAILED(begun)) {
                        error = std::format("xrBeginSession: {}", ResultString(m_impl->instance, begun));
                        return false;
                    }
                    m_sessionRunning = true;
                    m_stage = Stage::SessionRunning;
                } else if (changed.state == XR_SESSION_STATE_STOPPING) {
                    m_sessionRunning = false;
                    const XrResult ended = xrEndSession(m_impl->session);
                    m_calls.push_back({"xrEndSession", ResultString(m_impl->instance, ended),
                                       XR_SUCCEEDED(ended)});
                } else if (changed.state == XR_SESSION_STATE_EXITING ||
                           changed.state == XR_SESSION_STATE_LOSS_PENDING) {
                    m_calls.push_back({changed.state == XR_SESSION_STATE_EXITING
                                           ? "session state -> XR_SESSION_STATE_EXITING"
                                           : "session state -> XR_SESSION_STATE_LOSS_PENDING",
                                       "received", true});
                    m_sessionRunning = false;
                    m_impl->exitRequested = true;
                }
                break;
            }

            case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
                // THE headset-disconnect path. The runtime is going away and
                // every handle we hold is about to become invalid.
                m_calls.push_back({"event XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING", "received", true});
                m_sessionRunning = false;
                m_impl->exitRequested = true;
                break;

            default:
                break;
        }
    }

    return !m_impl->exitRequested;
}

bool SdkSession::SubmitFrame(ID3D11Texture2D* gameTexture, bool verifyCopy, FrameResult& result_,
                             std::string& error) {
    FrameTimings& timings = result_.timings;
    if (m_impl == nullptr || !m_sessionRunning) {
        error = "SubmitFrame while the session is not running";
        return false;
    }
    if (gameTexture == nullptr) {
        error = "SubmitFrame was given a null game texture";
        return false;
    }

    LARGE_INTEGER t0{}, t1{}, t2{}, t3{}, t4{}, t5{};

    // --- Wait --------------------------------------------------------------
    // Where the runtime throttles us to the display cadence. Section 5's
    // question about stalling is really a question about whether the copy adds
    // time on top of this, or hides inside it.
    QueryPerformanceCounter(&t0);
    XrFrameWaitInfo waitInfo{XR_TYPE_FRAME_WAIT_INFO};
    m_impl->frameState = XrFrameState{XR_TYPE_FRAME_STATE};
    XrResult result = xrWaitFrame(m_impl->session, &waitInfo, &m_impl->frameState);
    QueryPerformanceCounter(&t1);
    if (XR_FAILED(result)) {
        error = std::format("xrWaitFrame: {}", ResultString(m_impl->instance, result));
        return false;
    }
    timings.waitFrameMs = MillisecondsBetween(t0, t1);

    XrFrameBeginInfo beginInfo{XR_TYPE_FRAME_BEGIN_INFO};
    result = xrBeginFrame(m_impl->session, &beginInfo);
    if (XR_FAILED(result)) {
        error = std::format("xrBeginFrame: {}", ResultString(m_impl->instance, result));
        return false;
    }

    bool submittedLayer = false;
    XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    std::vector<XrCompositionLayerProjectionView> projectionViews;

    if (m_impl->frameState.shouldRender == XR_TRUE) {
        // --- Acquire -------------------------------------------------------
        QueryPerformanceCounter(&t2);
        uint32_t imageIndex = 0;
        XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        result = xrAcquireSwapchainImage(m_impl->swapchain, &acquireInfo, &imageIndex);
        if (XR_FAILED(result)) {
            error = std::format("xrAcquireSwapchainImage: {}", ResultString(m_impl->instance, result));
            return false;
        }

        // The runtime may still be reading the image the compositor showed two
        // frames ago. Writing before this returns is a race with the
        // compositor, and it is the reason a copy cannot simply follow acquire.
        XrSwapchainImageWaitInfo imageWait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        imageWait.timeout = XR_INFINITE_DURATION;
        result = xrWaitSwapchainImage(m_impl->swapchain, &imageWait);
        QueryPerformanceCounter(&t3);
        if (XR_FAILED(result)) {
            error = std::format("xrWaitSwapchainImage: {}", ResultString(m_impl->instance, result));
            return false;
        }
        timings.acquireWaitMs = MillisecondsBetween(t2, t3);

        // --- THE COPY ------------------------------------------------------
        // Model B in one line. The game's pixels into the runtime's texture, on
        // the game's immediate context, with no ownership changing hands.
        //
        // CopyResource returns void. There is no HRESULT and no failure signal
        // at all — an invalid copy is a silent no-op outside the debug layer,
        // which is why the mutation tests in the results document read the
        // destination back rather than trusting that this line ran.
        QueryPerformanceCounter(&t4);
        m_context->CopyResource(m_swapchainImages[imageIndex], gameTexture);
        QueryPerformanceCounter(&t5);
        timings.copyMs = MillisecondsBetween(t4, t5);

        // --- Did it actually happen? ---------------------------------------
        // Read one pixel back out of the runtime's texture while we still hold
        // it. Everything above this line reports success whether or not a
        // single byte moved, which is not a hypothetical: the first run of this
        // prototype submitted thirty frames of nothing and called it a pass.
        if (verifyCopy) {
            result_.verifyAttempted = true;

            D3D11_TEXTURE2D_DESC swapDesc{};
            m_swapchainImages[imageIndex]->GetDesc(&swapDesc);

            D3D11_TEXTURE2D_DESC probeDesc{};
            probeDesc.Width = 1;
            probeDesc.Height = 1;
            probeDesc.MipLevels = 1;
            probeDesc.ArraySize = 1;
            probeDesc.Format = swapDesc.Format;
            probeDesc.SampleDesc.Count = 1;
            probeDesc.Usage = D3D11_USAGE_STAGING;
            probeDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

            ComPtr<ID3D11Texture2D> probe;
            if (SUCCEEDED(m_device->CreateTexture2D(&probeDesc, nullptr, &probe))) {
                const D3D11_BOX box{0, 0, 0, 1, 1, 1};
                m_context->CopySubresourceRegion(probe.Get(), 0, 0, 0, 0,
                                                 m_swapchainImages[imageIndex], 0, &box);

                D3D11_MAPPED_SUBRESOURCE mapped{};
                if (SUCCEEDED(m_context->Map(probe.Get(), 0, D3D11_MAP_READ, 0, &mapped))) {
                    const auto* pixel = static_cast<const uint8_t*>(mapped.pData);
                    for (int channel = 0; channel < 4; ++channel) {
                        result_.sampledPixel[channel] = pixel[channel];
                    }
                    m_context->Unmap(probe.Get(), 0);

                    // The caller knows what colour it drew; this side only
                    // reports what arrived. Anything non-zero at least proves
                    // the destination was written, and the caller's comparison
                    // proves it was written with the right thing.
                    result_.copyVerified = true;
                }
            }
        }

        XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        result = xrReleaseSwapchainImage(m_impl->swapchain, &releaseInfo);
        if (XR_FAILED(result)) {
            error = std::format("xrReleaseSwapchainImage: {}", ResultString(m_impl->instance, result));
            return false;
        }

        // --- Layer ---------------------------------------------------------
        uint32_t viewCountOutput = 0;
        std::vector<XrView> views(m_viewCount, {XR_TYPE_VIEW});
        XrViewLocateInfo locateInfo{XR_TYPE_VIEW_LOCATE_INFO};
        locateInfo.viewConfigurationType = kViewConfig;
        locateInfo.displayTime = m_impl->frameState.predictedDisplayTime;
        locateInfo.space = m_impl->space;

        XrViewState viewState{XR_TYPE_VIEW_STATE};
        result = xrLocateViews(m_impl->session, &locateInfo, &viewState, m_viewCount,
                               &viewCountOutput, views.data());

        if (XR_SUCCEEDED(result) &&
            (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) != 0) {
            projectionViews.resize(viewCountOutput, {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW});
            const uint32_t eyeWidth = m_width / (m_viewCount > 0 ? m_viewCount : 1);

            for (uint32_t eye = 0; eye < viewCountOutput; ++eye) {
                projectionViews[eye].pose = views[eye].pose;
                projectionViews[eye].fov = views[eye].fov;
                projectionViews[eye].subImage.swapchain = m_impl->swapchain;
                projectionViews[eye].subImage.imageRect.offset = {
                    static_cast<int32_t>(eye * eyeWidth), 0};
                projectionViews[eye].subImage.imageRect.extent = {
                    static_cast<int32_t>(eyeWidth), static_cast<int32_t>(m_height)};
                projectionViews[eye].subImage.imageArrayIndex = 0;
            }

            layer.space = m_impl->space;
            layer.viewCount = static_cast<uint32_t>(projectionViews.size());
            layer.views = projectionViews.data();
            submittedLayer = true;
        }
    }

    // --- End ---------------------------------------------------------------
    const XrCompositionLayerBaseHeader* layers[] = {
        reinterpret_cast<const XrCompositionLayerBaseHeader*>(&layer)};

    XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
    endInfo.displayTime = m_impl->frameState.predictedDisplayTime;
    endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    endInfo.layerCount = submittedLayer ? 1u : 0u;
    endInfo.layers = submittedLayer ? layers : nullptr;

    LARGE_INTEGER e0{}, e1{};
    QueryPerformanceCounter(&e0);
    result = xrEndFrame(m_impl->session, &endInfo);
    QueryPerformanceCounter(&e1);
    if (XR_FAILED(result)) {
        error = std::format("xrEndFrame: {}", ResultString(m_impl->instance, result));
        return false;
    }
    timings.endFrameMs = MillisecondsBetween(e0, e1);

    ++m_framesSubmitted;
    m_stage = Stage::FrameSubmitted;
    return true;
}

void SdkSession::SetSwapchainWaitTimeoutNanos(int64_t nanos) noexcept {
    if (m_impl != nullptr) {
        m_impl->swapchainWaitNanos = nanos;
    }
}

void SdkSession::SetSimulatedSkipEvery(uint32_t everyNthFrame) noexcept {
    if (m_impl != nullptr) {
        m_impl->simulateSkipEvery = everyNthFrame;
    }
}

void SdkSession::SetSimulatedTimeoutEvery(uint32_t everyNthFrame) noexcept {
    if (m_impl != nullptr) {
        m_impl->simulateTimeoutEvery = everyNthFrame;
    }
}

bool SdkSession::WaitFrame(FrameHandoff& handoff, std::string& error) {
    if (m_impl == nullptr || !m_sessionRunning) {
        error = "WaitFrame while the session is not running";
        return false;
    }

    // xrWaitFrame and xrLocateViews. No D3D, no swapchain, no lock of any kind
    // — which is what makes this callable from a simulation thread that has
    // never touched the immediate context. xrLocateViews reads tracking state
    // against a display time; it touches no graphics resource and needs no
    // context, so it does not weaken that property.
    XrFrameWaitInfo waitInfo{XR_TYPE_FRAME_WAIT_INFO};
    XrFrameState frameState{XR_TYPE_FRAME_STATE};
    const XrResult result = xrWaitFrame(m_impl->session, &waitInfo, &frameState);
    if (XR_FAILED(result)) {
        error = std::format("xrWaitFrame: {}", ResultString(m_impl->instance, result));
        return false;
    }

    m_impl->lastPredictedDisplayTime = frameState.predictedDisplayTime;

    // Fault injection, applied to the frame state BEFORE anything reads it.
    //
    // Overwriting the runtime's answer here rather than special-casing further
    // down means every consumer sees an ordinary declined frame: the locate is
    // skipped, the handoff reports a skip, the cached frame state carries the
    // false through to submit, and the copy is bypassed. A flag consulted at
    // each of those points instead would be testing the flag, not the path.
    ++m_impl->skipCounter;
    if (m_impl->simulateSkipEvery != 0 &&
        (m_impl->skipCounter % m_impl->simulateSkipEvery) == 0) {
        frameState.shouldRender = XR_FALSE;
    }

    // --- Locate, once ------------------------------------------------------
    // These poses go two places from here: out to the game, which renders from
    // them, and into the pending-frame cache, from which the composition layer
    // is built at submit time. Both consumers see the SAME numbers, which is
    // the property the whole locate-once design exists to guarantee.
    //
    // Skipped entirely when the runtime says not to render. There is nothing to
    // locate a pose FOR on a frame that will not be displayed, and asking anyway
    // would spend a tracking query per frame while the headset sits idle.
    Impl::PendingFrame pending;
    pending.frameState = frameState;

    if (frameState.shouldRender == XR_TRUE) {
        pending.views.assign(m_viewCount, XrView{XR_TYPE_VIEW});

        XrViewLocateInfo locateInfo{XR_TYPE_VIEW_LOCATE_INFO};
        locateInfo.viewConfigurationType = kViewConfig;
        locateInfo.displayTime = frameState.predictedDisplayTime;
        locateInfo.space = m_impl->space;

        uint32_t viewCountOutput = 0;
        XrViewState viewState{XR_TYPE_VIEW_STATE};
        const XrResult located = xrLocateViews(m_impl->session, &locateInfo, &viewState,
                                               m_viewCount, &viewCountOutput, pending.views.data());

        // A located view is only usable when the runtime says BOTH bits are
        // set. Orientation alone is not enough for a projection layer: the
        // position would be whatever the struct was left at, and a head that
        // rotates correctly while sitting at a stale point in space is the kind
        // of wrongness that reads as a rendering bug rather than a tracking one.
        //
        // Not an error. Tracking drops for ordinary reasons — the headset comes
        // off, the guardian is being redrawn, the runtime is recentring — and
        // the frame is reported as skipped so the caller keeps ticking and
        // submits the token. Failing the call would tear down a session over a
        // condition that resolves itself in a few frames.
        constexpr XrViewStateFlags kUsable =
            XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT;

        pending.posesValid = XR_SUCCEEDED(located) &&
                             (viewState.viewStateFlags & kUsable) == kUsable &&
                             viewCountOutput > 0;

        if (pending.posesValid) {
            pending.views.resize(viewCountOutput);
        } else {
            pending.views.clear();
        }
    }

    handoff.predictedDisplayTime = frameState.predictedDisplayTime;
    handoff.posesValid = pending.posesValid;

    // shouldRender is the AND of what the runtime asked for and what we can
    // actually locate. The public invariant on NexVR_FrameState says these move
    // together, and this is the single place that makes that true.
    handoff.shouldRender = frameState.shouldRender == XR_TRUE && pending.posesValid;
    handoff.viewCount = handoff.shouldRender ? static_cast<uint32_t>(pending.views.size()) : 0u;

    for (std::size_t eye = 0; eye < pending.views.size() && eye < std::size(handoff.views); ++eye) {
        handoff.views[eye] = ToPublicView(pending.views[eye]);
    }

    std::lock_guard<std::mutex> guard(m_impl->handoffMutex);
    handoff.token = m_impl->nextToken++;
    m_impl->pendingFrames.emplace(handoff.token, std::move(pending));
    return true;
}

bool SdkSession::TokenShouldRender(uint64_t token, bool& shouldRender) const {
    if (m_impl == nullptr) {
        return false;
    }

    std::lock_guard<std::mutex> guard(m_impl->handoffMutex);
    const auto found = m_impl->pendingFrames.find(token);
    if (found == m_impl->pendingFrames.end()) {
        return false;
    }

    // The same conjunction WaitFrame reported to the game. Asking only about
    // frameState.shouldRender would answer a different question than the one
    // the caller was told the answer to.
    shouldRender = found->second.frameState.shouldRender == XR_TRUE && found->second.posesValid;
    return true;
}

bool SdkSession::SubmitFrameThreaded(uint64_t token, ID3D11Texture2D* gameTexture, bool verifyCopy,
                                     FrameResult& result_, std::string& error,
                                     const DepthSubmission* depth) {
    if (m_impl == nullptr || !m_sessionRunning) {
        error = "SubmitFrameThreaded while the session is not running";
        return false;
    }

    // --- Redeem the token --------------------------------------------------
    // A token that is not here was already submitted, never issued, or belongs
    // to a session that has since restarted. With the wait and the begin on
    // different threads there is nothing else that would notice.
    Impl::PendingFrame pending;
    {
        std::lock_guard<std::mutex> guard(m_impl->handoffMutex);
        const auto found = m_impl->pendingFrames.find(token);
        if (found == m_impl->pendingFrames.end()) {
            error = std::format("frame token {} is not outstanding", token);
            return false;
        }

        // Checked BEFORE the erase, so a rejected call leaves the token
        // outstanding and the caller can retry with the real texture. Erasing
        // first would turn a recoverable argument mistake into a permanently
        // lost frame: xrWaitFrame has already happened and xrBeginFrame has
        // not, and nothing downstream would ever close that gap.
        //
        // A null texture is legal on a frame that is not being rendered — see
        // the contract on NexVR_SubmitFrameDX11 — and this is the guard for
        // every other case. SubmitFrameThreaded is a public entry point, so it
        // owns its own null safety rather than trusting the one caller that
        // currently happens to check first.
        if (gameTexture == nullptr && found->second.frameState.shouldRender == XR_TRUE &&
            found->second.posesValid) {
            error = std::format(
                "frame token {} is a rendered frame but no texture was supplied; the token is "
                "still outstanding",
                token);
            return false;
        }

        pending = std::move(found->second);
        m_impl->pendingFrames.erase(found);
    }

    const XrFrameState frameState = pending.frameState;

    XrFrameBeginInfo beginInfo{XR_TYPE_FRAME_BEGIN_INFO};
    XrResult result = xrBeginFrame(m_impl->session, &beginInfo);
    if (XR_FAILED(result) && result != XR_FRAME_DISCARDED) {
        error = std::format("xrBeginFrame: {}", ResultString(m_impl->instance, result));
        return false;
    }

    bool submittedLayer = false;
    XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    std::vector<XrCompositionLayerProjectionView> projectionViews;

    // posesValid as well as shouldRender, because WaitFrame reported this frame
    // to the game as skipped when the poses were unlocatable — so the game did
    // not render it, and there is nothing here worth copying or displaying. The
    // frame still gets begun and ended below; only the image work is skipped.
    if (frameState.shouldRender == XR_TRUE && pending.posesValid) {
        LARGE_INTEGER a0{}, a1{};
        QueryPerformanceCounter(&a0);

        // =================================================================
        // STEP 1 & 2 — NO D3D11 LOCK IS HELD HERE. THIS IS THE WHOLE POINT.
        // =================================================================
        // Both calls below can block on the compositor. Holding the immediate
        // context lock across them would mean a compositor stall freezes every
        // engine thread that needs the context, including whichever one would
        // run the teardown — a VR hiccup becoming an unkillable game.
        uint32_t imageIndex = 0;
        if (m_impl->outstandingImage >= 0) {
            // A previous frame acquired this image and timed out waiting. The
            // runtime still considers it ours, so retry rather than acquiring
            // another and leaking the slot.
            imageIndex = static_cast<uint32_t>(m_impl->outstandingImage);
        } else {
            XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            result = xrAcquireSwapchainImage(m_impl->swapchain, &acquireInfo, &imageIndex);
            if (XR_FAILED(result)) {
                error = std::format("xrAcquireSwapchainImage: {}",
                                    ResultString(m_impl->instance, result));
                return false;
            }
        }

        // Fault injection REPLACES the wait rather than following it.
        //
        // The first version called xrWaitSwapchainImage and then overrode a
        // successful result, which left the image waited-on while the code
        // believed it was not — and the retry next frame earned an
        // XR_ERROR_CALL_ORDER_INVALID from the runtime. That was a bug in the
        // injection, but it proved something useful: SteamVR does enforce
        // swapchain call ordering, so an SDK that mismanages acquire/wait/release
        // gets a hard error rather than silent corruption.
        //
        // A genuine XR_TIMEOUT_EXPIRED leaves the image acquired and NOT waited.
        // Skipping the call entirely reproduces that state exactly.
        ++m_impl->waitCounter;
        const bool injected = m_impl->simulateTimeoutEvery != 0 &&
                              (m_impl->waitCounter % m_impl->simulateTimeoutEvery) == 0;

        if (injected) {
            result = XR_TIMEOUT_EXPIRED;
        } else {
            XrSwapchainImageWaitInfo imageWait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            imageWait.timeout = m_impl->swapchainWaitNanos;  // FINITE. Never XR_INFINITE_DURATION.
            result = xrWaitSwapchainImage(m_impl->swapchain, &imageWait);
        }
        QueryPerformanceCounter(&a1);
        result_.timings.acquireWaitMs = MillisecondsBetween(a0, a1);

        if (result == XR_TIMEOUT_EXPIRED) {
            // Drop the frame and keep going. The image stays acquired for the
            // next attempt, and the session is untouched — a timeout is a
            // hiccup, not a reason to tear down a user's VR session.
            m_impl->outstandingImage = static_cast<int32_t>(imageIndex);
            result_.droppedOnTimeout = true;
            ++m_framesDropped;

            XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
            endInfo.displayTime = frameState.predictedDisplayTime;
            endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
            endInfo.layerCount = 0;
            endInfo.layers = nullptr;
            const XrResult ended = xrEndFrame(m_impl->session, &endInfo);
            if (XR_FAILED(ended)) {
                error = std::format("xrEndFrame after timeout: {}",
                                    ResultString(m_impl->instance, ended));
                return false;
            }
            return true;
        }

        if (XR_FAILED(result)) {
            error = std::format("xrWaitSwapchainImage: {}", ResultString(m_impl->instance, result));
            return false;
        }
        m_impl->outstandingImage = -1;

        // =================================================================
        // STEP 3-5 — the lock covers the D3D work and nothing else.
        // =================================================================
        // --- Depth image, acquired OUTSIDE the D3D lock -------------------
        //
        // Same ordering rule as the colour image and for the same Phase 0.9
        // reason: acquire and wait can both block on the compositor, and holding
        // the immediate-context lock across a compositor stall turns a VR hiccup
        // into a frozen engine.
        //
        // Every failure here abandons DEPTH ONLY. The colour frame still goes
        // out, because a game that submitted depth as an optional extra must not
        // lose its frame when the optional extra is unavailable.
        bool depthReady = false;
        uint32_t depthImageIndex = 0;
        if (depth != nullptr && depth->texture != nullptr && m_depthUsable) {
            if (m_impl->outstandingDepthImage >= 0) {
                depthImageIndex = static_cast<uint32_t>(m_impl->outstandingDepthImage);
                depthReady = true;
            } else {
                XrSwapchainImageAcquireInfo depthAcquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
                depthReady = XR_SUCCEEDED(xrAcquireSwapchainImage(
                    m_impl->depthSwapchain, &depthAcquire, &depthImageIndex));
            }

            if (depthReady) {
                XrSwapchainImageWaitInfo depthWait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
                depthWait.timeout = m_impl->swapchainWaitNanos;  // FINITE, as above.
                const XrResult waited = xrWaitSwapchainImage(m_impl->depthSwapchain, &depthWait);
                if (waited == XR_TIMEOUT_EXPIRED) {
                    // Keep the slot for next frame rather than acquiring another
                    // and leaking it, exactly as the colour path does.
                    m_impl->outstandingDepthImage = static_cast<int32_t>(depthImageIndex);
                    depthReady = false;
                } else if (XR_FAILED(waited)) {
                    depthReady = false;
                } else {
                    m_impl->outstandingDepthImage = -1;
                }
            }
        }

        LARGE_INTEGER l0{}, l1{}, c0{}, c1{};
        QueryPerformanceCounter(&l0);
        if (m_multithread) {
            m_multithread->Enter();
        }
        QueryPerformanceCounter(&l1);
        result_.timings.lockWaitMsInternal = MillisecondsBetween(l0, l1);
        result_.lockWaitMs = result_.timings.lockWaitMsInternal;

        QueryPerformanceCounter(&c0);
        m_context->CopyResource(m_swapchainImages[imageIndex], gameTexture);

        // Inside the SAME lock acquisition as the colour copy, deliberately.
        //
        // Two lock round-trips per frame would double the contention with the
        // game's own context use for no benefit, and would let the game's draws
        // interleave BETWEEN the colour and depth copies — producing a pair that
        // came from two different moments in the frame. The compositor treats
        // them as one observation, so they have to be taken as one.
        if (depthReady) {
            m_context->CopyResource(m_depthSwapchainImages[depthImageIndex], depth->texture);
        }
        QueryPerformanceCounter(&c1);
        result_.timings.copyMs = MillisecondsBetween(c0, c1);

        if (verifyCopy) {
            result_.verifyAttempted = true;
            D3D11_TEXTURE2D_DESC swapDesc{};
            m_swapchainImages[imageIndex]->GetDesc(&swapDesc);

            D3D11_TEXTURE2D_DESC probeDesc{};
            probeDesc.Width = 1;
            probeDesc.Height = 1;
            probeDesc.MipLevels = 1;
            probeDesc.ArraySize = 1;
            probeDesc.Format = swapDesc.Format;
            probeDesc.SampleDesc.Count = 1;
            probeDesc.Usage = D3D11_USAGE_STAGING;
            probeDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

            ComPtr<ID3D11Texture2D> probe;
            if (SUCCEEDED(m_device->CreateTexture2D(&probeDesc, nullptr, &probe))) {
                const D3D11_BOX box{0, 0, 0, 1, 1, 1};
                m_context->CopySubresourceRegion(probe.Get(), 0, 0, 0, 0,
                                                 m_swapchainImages[imageIndex], 0, &box);
                D3D11_MAPPED_SUBRESOURCE mapped{};
                if (SUCCEEDED(m_context->Map(probe.Get(), 0, D3D11_MAP_READ, 0, &mapped))) {
                    const auto* pixel = static_cast<const uint8_t*>(mapped.pData);
                    for (int channel = 0; channel < 4; ++channel) {
                        result_.sampledPixel[channel] = pixel[channel];
                    }
                    m_context->Unmap(probe.Get(), 0);
                    result_.copyVerified = true;
                }
            }
        }

        if (m_multithread) {
            m_multithread->Leave();
        }

        // --- Step 6, back outside the lock ---------------------------------
        XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};

        // Released before the colour image, so that a failure to release depth
        // cannot strand the colour image the compositor is actually waiting on.
        if (depthReady) {
            XrSwapchainImageReleaseInfo depthRelease{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            if (XR_FAILED(xrReleaseSwapchainImage(m_impl->depthSwapchain, &depthRelease))) {
                // Depth is dropped for this frame; the colour frame still goes.
                depthReady = false;
            }
        }

        result = xrReleaseSwapchainImage(m_impl->swapchain, &releaseInfo);
        if (XR_FAILED(result)) {
            error = std::format("xrReleaseSwapchainImage: {}", ResultString(m_impl->instance, result));
            return false;
        }

        // --- Layer, from the poses the GAME rendered from -------------------
        //
        // No xrLocateViews here. It used to sit at exactly this point, and the
        // views it produced were correct, current, and wrong for the job.
        //
        // The compositor reprojects the submitted image against the pose the
        // layer names. Locating again here yields a pose roughly one frame
        // newer than the one WaitFrame handed the game, so the compositor would
        // correct for head motion that these pixels already contain — the world
        // shearing slightly against head movement, every frame, in a session
        // where every OpenXR call returns success and no diagnostic fires.
        //
        // These are the same XrView values WaitFrame located and cached against
        // this token, and the same ones the game received as
        // NexVR_FrameState::views. Pixels and pose now come from one sample.
        if (pending.posesValid && !pending.views.empty()) {
            const auto viewCountOutput = static_cast<uint32_t>(pending.views.size());
            projectionViews.resize(viewCountOutput, {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW});

            // Sized ONCE, before the loop, and never resized inside it.
            //
            // Each projection view holds a raw `next` pointer into this vector,
            // and a vector that reallocates mid-loop leaves every pointer taken
            // before the reallocation dangling. xrEndFrame would then read freed
            // memory — which is the kind of defect that survives testing because
            // freed memory usually still contains the right values.
            std::vector<XrCompositionLayerDepthInfoKHR> depthInfos;
            if (depthReady) {
                depthInfos.assign(viewCountOutput,
                                  XrCompositionLayerDepthInfoKHR{
                                      XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR});
            }

            const uint32_t eyeWidth = m_width / (m_viewCount > 0 ? m_viewCount : 1);
            for (uint32_t eye = 0; eye < viewCountOutput; ++eye) {
                projectionViews[eye].pose = pending.views[eye].pose;
                projectionViews[eye].fov = pending.views[eye].fov;
                projectionViews[eye].subImage.swapchain = m_impl->swapchain;
                projectionViews[eye].subImage.imageRect.offset = {
                    static_cast<int32_t>(eye * eyeWidth), 0};
                projectionViews[eye].subImage.imageRect.extent = {
                    static_cast<int32_t>(eyeWidth), static_cast<int32_t>(m_height)};
                projectionViews[eye].subImage.imageArrayIndex = 0;

                if (depthReady) {
                    XrCompositionLayerDepthInfoKHR& info = depthInfos[eye];

                    // The SAME rect as the colour view. The two buffers are one
                    // side-by-side image each, and naming a different region
                    // here would hand the compositor depth from the wrong eye —
                    // which reads as broken occlusion, not as a wrong rectangle.
                    info.subImage.swapchain = m_impl->depthSwapchain;
                    info.subImage.imageRect = projectionViews[eye].subImage.imageRect;
                    info.subImage.imageArrayIndex = 0;

                    // minDepth and maxDepth describe the RANGE OF VALUES in the
                    // buffer, not the clip planes — they are the viewport's
                    // depth range, almost always 0..1 regardless of reverse-Z.
                    // Reverse-Z is expressed by nearZ > farZ below, not by
                    // swapping these, and swapping them instead is the classic
                    // way to get a depth layer that is subtly wrong everywhere.
                    info.minDepth = 0.0f;
                    info.maxDepth = 1.0f;

                    info.nearZ = depth->nearZ;
                    info.farZ = depth->farZ;

                    projectionViews[eye].next = &info;
                }
            }

            layer.space = m_impl->space;
            layer.viewCount = static_cast<uint32_t>(projectionViews.size());
            layer.views = projectionViews.data();
            submittedLayer = true;

            // xrEndFrame reads the chained depth structs, so it must happen
            // while `depthInfos` is still alive. It is called below, outside
            // this block, and the vector would be destroyed at the closing brace
            // — so the frame is ended HERE when depth is attached.
            if (depthReady) {
                const XrCompositionLayerBaseHeader* depthLayers[] = {
                    reinterpret_cast<const XrCompositionLayerBaseHeader*>(&layer)};

                XrFrameEndInfo depthEnd{XR_TYPE_FRAME_END_INFO};
                depthEnd.displayTime = frameState.predictedDisplayTime;
                depthEnd.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
                depthEnd.layerCount = 1;
                depthEnd.layers = depthLayers;

                LARGE_INTEGER d0{}, d1{};
                QueryPerformanceCounter(&d0);
                const XrResult ended = xrEndFrame(m_impl->session, &depthEnd);
                QueryPerformanceCounter(&d1);
                if (XR_FAILED(ended)) {
                    error = std::format("xrEndFrame (with depth): {}",
                                        ResultString(m_impl->instance, ended));
                    return false;
                }
                result_.timings.endFrameMs = MillisecondsBetween(d0, d1);

                ++m_framesSubmitted;
                m_stage = Stage::FrameSubmitted;
                return true;
            }
        }
    }

    const XrCompositionLayerBaseHeader* layers[] = {
        reinterpret_cast<const XrCompositionLayerBaseHeader*>(&layer)};

    XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
    endInfo.displayTime = frameState.predictedDisplayTime;
    endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    endInfo.layerCount = submittedLayer ? 1u : 0u;
    endInfo.layers = submittedLayer ? layers : nullptr;

    LARGE_INTEGER e0{}, e1{};
    QueryPerformanceCounter(&e0);
    result = xrEndFrame(m_impl->session, &endInfo);
    QueryPerformanceCounter(&e1);
    if (XR_FAILED(result)) {
        error = std::format("xrEndFrame: {}", ResultString(m_impl->instance, result));
        return false;
    }
    result_.timings.endFrameMs = MillisecondsBetween(e0, e1);

    ++m_framesSubmitted;
    m_stage = Stage::FrameSubmitted;
    return true;
}

bool SdkSession::SubmitFrameThreadedDX12(uint64_t token, ID3D12Resource* gameTexture,
                                         D3D12_RESOURCE_STATES colorState, bool verifyCopy,
                                         FrameResult& result_, std::string& error,
                                         const DepthSubmissionDX12* depth) {
    if (m_impl == nullptr || !m_sessionRunning) {
        error = "SubmitFrameThreadedDX12 while the session is not running";
        return false;
    }

    Impl::PendingFrame pending;
    {
        std::lock_guard<std::mutex> lock(m_impl->handoffMutex);
        auto found = m_impl->pendingFrames.find(token);
        if (found == m_impl->pendingFrames.end()) {
            error = std::format("frame token {} is not outstanding", token);
            return false;
        }

        if (gameTexture == nullptr && found->second.frameState.shouldRender == XR_TRUE &&
            found->second.posesValid) {
            error = std::format(
                "frame token {} is a rendered frame but no texture was supplied; the token is "
                "still outstanding",
                token);
            return false;
        }

        pending = std::move(found->second);
        m_impl->pendingFrames.erase(found);
    }

    const XrFrameState frameState = pending.frameState;

    XrFrameBeginInfo beginInfo{XR_TYPE_FRAME_BEGIN_INFO};
    XrResult result = xrBeginFrame(m_impl->session, &beginInfo);
    if (XR_FAILED(result) && result != XR_FRAME_DISCARDED) {
        error = std::format("xrBeginFrame: {}", ResultString(m_impl->instance, result));
        return false;
    }

    bool submittedLayer = false;
    XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    std::vector<XrCompositionLayerProjectionView> projectionViews;

    if (frameState.shouldRender == XR_TRUE && pending.posesValid) {
        LARGE_INTEGER a0{}, a1{};
        QueryPerformanceCounter(&a0);

        uint32_t imageIndex = 0;
        if (m_impl->outstandingImage >= 0) {
            imageIndex = static_cast<uint32_t>(m_impl->outstandingImage);
        } else {
            XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            result = xrAcquireSwapchainImage(m_impl->swapchain, &acquireInfo, &imageIndex);
            if (XR_FAILED(result)) {
                error = std::format("xrAcquireSwapchainImage: {}",
                                    ResultString(m_impl->instance, result));
                return false;
            }
        }

        ++m_impl->waitCounter;
        const bool injected = m_impl->simulateTimeoutEvery != 0 &&
                              (m_impl->waitCounter % m_impl->simulateTimeoutEvery) == 0;

        if (injected) {
            result = XR_TIMEOUT_EXPIRED;
        } else {
            XrSwapchainImageWaitInfo imageWait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            imageWait.timeout = m_impl->swapchainWaitNanos;
            result = xrWaitSwapchainImage(m_impl->swapchain, &imageWait);
        }
        QueryPerformanceCounter(&a1);
        result_.timings.acquireWaitMs = MillisecondsBetween(a0, a1);

        if (result == XR_TIMEOUT_EXPIRED) {
            m_impl->outstandingImage = static_cast<int32_t>(imageIndex);
            result_.droppedOnTimeout = true;
            ++m_framesDropped;

            XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
            endInfo.displayTime = frameState.predictedDisplayTime;
            endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
            endInfo.layerCount = 0;
            endInfo.layers = nullptr;
            const XrResult ended = xrEndFrame(m_impl->session, &endInfo);
            if (XR_FAILED(ended)) {
                error = std::format("xrEndFrame after timeout: {}",
                                    ResultString(m_impl->instance, ended));
                return false;
            }
            return true;
        }

        if (XR_FAILED(result)) {
            error = std::format("xrWaitSwapchainImage: {}", ResultString(m_impl->instance, result));
            return false;
        }
        m_impl->outstandingImage = -1;

        bool depthReady = false;
        uint32_t depthImageIndex = 0;
        if (depth != nullptr && depth->texture != nullptr && m_depthUsable) {
            if (m_impl->outstandingDepthImage >= 0) {
                depthImageIndex = static_cast<uint32_t>(m_impl->outstandingDepthImage);
                depthReady = true;
            } else {
                XrSwapchainImageAcquireInfo depthAcquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
                depthReady = XR_SUCCEEDED(xrAcquireSwapchainImage(
                    m_impl->depthSwapchain, &depthAcquire, &depthImageIndex));
            }

            if (depthReady) {
                XrSwapchainImageWaitInfo depthWait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
                depthWait.timeout = m_impl->swapchainWaitNanos;
                const XrResult waited = xrWaitSwapchainImage(m_impl->depthSwapchain, &depthWait);
                if (waited == XR_TIMEOUT_EXPIRED) {
                    m_impl->outstandingDepthImage = static_cast<int32_t>(depthImageIndex);
                    depthReady = false;
                } else if (XR_FAILED(waited)) {
                    depthReady = false;
                } else {
                    m_impl->outstandingDepthImage = -1;
                }
            }
        }

        LARGE_INTEGER c0{}, c1{};
        QueryPerformanceCounter(&c0);

        if (m_d3d12Fence && m_d3d12Fence->GetCompletedValue() < m_d3d12FenceValue) {
            m_d3d12Fence->SetEventOnCompletion(m_d3d12FenceValue, m_d3d12FenceEvent);
            WaitForSingleObject(m_d3d12FenceEvent, INFINITE);
        }

        m_d3d12CommandAllocator->Reset();
        m_d3d12CommandList->Reset(m_d3d12CommandAllocator.Get(), nullptr);

        std::vector<D3D12_RESOURCE_BARRIER> preBarriers;

        D3D12_RESOURCE_BARRIER swapchainPre{};
        swapchainPre.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        swapchainPre.Transition.pResource = m_d3d12SwapchainImages[imageIndex];
        swapchainPre.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        swapchainPre.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        swapchainPre.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        preBarriers.push_back(swapchainPre);

        if (colorState != D3D12_RESOURCE_STATE_COPY_SOURCE) {
            D3D12_RESOURCE_BARRIER colorPre{};
            colorPre.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            colorPre.Transition.pResource = gameTexture;
            colorPre.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            colorPre.Transition.StateBefore = colorState;
            colorPre.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
            preBarriers.push_back(colorPre);
        }

        if (depthReady) {
            D3D12_RESOURCE_BARRIER depthSwapPre{};
            depthSwapPre.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            depthSwapPre.Transition.pResource = m_d3d12DepthSwapchainImages[depthImageIndex];
            depthSwapPre.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            depthSwapPre.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
            depthSwapPre.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            preBarriers.push_back(depthSwapPre);

            if (depth->state != D3D12_RESOURCE_STATE_COPY_SOURCE) {
                D3D12_RESOURCE_BARRIER depthPre{};
                depthPre.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                depthPre.Transition.pResource = depth->texture;
                depthPre.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                depthPre.Transition.StateBefore = depth->state;
                depthPre.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
                preBarriers.push_back(depthPre);
            }
        }

        m_d3d12CommandList->ResourceBarrier(static_cast<uint32_t>(preBarriers.size()), preBarriers.data());

        m_d3d12CommandList->CopyResource(m_d3d12SwapchainImages[imageIndex], gameTexture);
        if (depthReady) {
            m_d3d12CommandList->CopyResource(m_d3d12DepthSwapchainImages[depthImageIndex], depth->texture);
        }

        if (verifyCopy) {
            result_.verifyAttempted = true;
            D3D12_RESOURCE_DESC swapDesc = m_d3d12SwapchainImages[imageIndex]->GetDesc();
            D3D12_HEAP_PROPERTIES readbackHeapProps{};
            readbackHeapProps.Type = D3D12_HEAP_TYPE_READBACK;
            D3D12_RESOURCE_DESC bufferDesc{};
            bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            bufferDesc.Width = 256;
            bufferDesc.Height = 1;
            bufferDesc.DepthOrArraySize = 1;
            bufferDesc.MipLevels = 1;
            bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
            bufferDesc.SampleDesc.Count = 1;
            bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

            ComPtr<ID3D12Resource> readbackBuffer;
            if (SUCCEEDED(m_d3d12Device->CreateCommittedResource(
                    &readbackHeapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
                    D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readbackBuffer)))) {
                D3D12_RESOURCE_BARRIER readbackPre{};
                readbackPre.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                readbackPre.Transition.pResource = m_d3d12SwapchainImages[imageIndex];
                readbackPre.Transition.Subresource = 0;
                readbackPre.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
                readbackPre.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
                m_d3d12CommandList->ResourceBarrier(1, &readbackPre);

                D3D12_TEXTURE_COPY_LOCATION dstLoc{};
                dstLoc.pResource = readbackBuffer.Get();
                dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                dstLoc.PlacedFootprint.Offset = 0;
                dstLoc.PlacedFootprint.Footprint.Width = 1;
                dstLoc.PlacedFootprint.Footprint.Height = 1;
                dstLoc.PlacedFootprint.Footprint.Depth = 1;
                dstLoc.PlacedFootprint.Footprint.RowPitch = 256;
                dstLoc.PlacedFootprint.Footprint.Format = swapDesc.Format;

                D3D12_TEXTURE_COPY_LOCATION srcLoc{};
                srcLoc.pResource = m_d3d12SwapchainImages[imageIndex];
                srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                srcLoc.SubresourceIndex = 0;

                D3D12_BOX box{0, 0, 0, 1, 1, 1};
                m_d3d12CommandList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, &box);

                D3D12_RESOURCE_BARRIER readbackPost{};
                readbackPost.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                readbackPost.Transition.pResource = m_d3d12SwapchainImages[imageIndex];
                readbackPost.Transition.Subresource = 0;
                readbackPost.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
                readbackPost.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
                m_d3d12CommandList->ResourceBarrier(1, &readbackPost);

                m_d3d12CommandList->Close();
                ID3D12CommandList* lists[] = { m_d3d12CommandList.Get() };
                m_d3d12Queue->ExecuteCommandLists(1, lists);
                m_d3d12FenceValue++;
                m_d3d12Queue->Signal(m_d3d12Fence.Get(), m_d3d12FenceValue);
                m_d3d12Fence->SetEventOnCompletion(m_d3d12FenceValue, m_d3d12FenceEvent);
                WaitForSingleObject(m_d3d12FenceEvent, INFINITE);

                void* pMapped = nullptr;
                if (SUCCEEDED(readbackBuffer->Map(0, nullptr, &pMapped)) && pMapped != nullptr) {
                    const auto* pixel = static_cast<const uint8_t*>(pMapped);
                    for (int c = 0; c < 4; ++c) {
                        result_.sampledPixel[c] = pixel[c];
                    }
                    readbackBuffer->Unmap(0, nullptr);
                    result_.copyVerified = true;
                }

                m_d3d12CommandAllocator->Reset();
                m_d3d12CommandList->Reset(m_d3d12CommandAllocator.Get(), nullptr);
            }
        }

        std::vector<D3D12_RESOURCE_BARRIER> postBarriers;

        D3D12_RESOURCE_BARRIER swapchainPost{};
        swapchainPost.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        swapchainPost.Transition.pResource = m_d3d12SwapchainImages[imageIndex];
        swapchainPost.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        swapchainPost.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        swapchainPost.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        postBarriers.push_back(swapchainPost);

        if (colorState != D3D12_RESOURCE_STATE_COPY_SOURCE) {
            D3D12_RESOURCE_BARRIER colorPost{};
            colorPost.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            colorPost.Transition.pResource = gameTexture;
            colorPost.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            colorPost.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
            colorPost.Transition.StateAfter = colorState;
            postBarriers.push_back(colorPost);
        }

        if (depthReady) {
            D3D12_RESOURCE_BARRIER depthSwapPost{};
            depthSwapPost.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            depthSwapPost.Transition.pResource = m_d3d12DepthSwapchainImages[depthImageIndex];
            depthSwapPost.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            depthSwapPost.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            depthSwapPost.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
            postBarriers.push_back(depthSwapPost);

            if (depth->state != D3D12_RESOURCE_STATE_COPY_SOURCE) {
                D3D12_RESOURCE_BARRIER depthPost{};
                depthPost.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                depthPost.Transition.pResource = depth->texture;
                depthPost.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                depthPost.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
                depthPost.Transition.StateAfter = depth->state;
                postBarriers.push_back(depthPost);
            }
        }

        m_d3d12CommandList->ResourceBarrier(static_cast<uint32_t>(postBarriers.size()), postBarriers.data());

        m_d3d12CommandList->Close();
        ID3D12CommandList* lists[] = { m_d3d12CommandList.Get() };
        m_d3d12Queue->ExecuteCommandLists(1, lists);
        m_d3d12FenceValue++;
        m_d3d12Queue->Signal(m_d3d12Fence.Get(), m_d3d12FenceValue);

        QueryPerformanceCounter(&c1);
        result_.timings.copyMs = MillisecondsBetween(c0, c1);

        if (depthReady) {
            XrSwapchainImageReleaseInfo depthRelease{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            if (XR_FAILED(xrReleaseSwapchainImage(m_impl->depthSwapchain, &depthRelease))) {
                depthReady = false;
            }
        }

        XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        result = xrReleaseSwapchainImage(m_impl->swapchain, &releaseInfo);
        if (XR_FAILED(result)) {
            error = std::format("xrReleaseSwapchainImage: {}", ResultString(m_impl->instance, result));
            return false;
        }

        if (pending.posesValid && !pending.views.empty()) {
            const auto viewCountOutput = static_cast<uint32_t>(pending.views.size());
            projectionViews.resize(viewCountOutput, {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW});

            std::vector<XrCompositionLayerDepthInfoKHR> depthInfos;
            if (depthReady) {
                depthInfos.assign(viewCountOutput,
                                  XrCompositionLayerDepthInfoKHR{
                                      XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR});
            }

            const uint32_t eyeWidth = m_width / (m_viewCount > 0 ? m_viewCount : 1);
            for (uint32_t eye = 0; eye < viewCountOutput; ++eye) {
                projectionViews[eye].pose = pending.views[eye].pose;
                projectionViews[eye].fov = pending.views[eye].fov;
                projectionViews[eye].subImage.swapchain = m_impl->swapchain;
                projectionViews[eye].subImage.imageRect.offset = {
                    static_cast<int32_t>(eye * eyeWidth), 0};
                projectionViews[eye].subImage.imageRect.extent = {
                    static_cast<int32_t>(eyeWidth), static_cast<int32_t>(m_height)};
                projectionViews[eye].subImage.imageArrayIndex = 0;

                if (depthReady) {
                    XrCompositionLayerDepthInfoKHR& info = depthInfos[eye];
                    info.subImage.swapchain = m_impl->depthSwapchain;
                    info.subImage.imageRect = projectionViews[eye].subImage.imageRect;
                    info.subImage.imageArrayIndex = 0;
                    info.minDepth = 0.0f;
                    info.maxDepth = 1.0f;
                    info.nearZ = depth->nearZ;
                    info.farZ = depth->farZ;
                    projectionViews[eye].next = &info;
                }
            }

            layer.space = m_impl->space;
            layer.viewCount = static_cast<uint32_t>(projectionViews.size());
            layer.views = projectionViews.data();
            submittedLayer = true;

            if (depthReady) {
                const XrCompositionLayerBaseHeader* depthLayers[] = {
                    reinterpret_cast<const XrCompositionLayerBaseHeader*>(&layer)};

                XrFrameEndInfo depthEnd{XR_TYPE_FRAME_END_INFO};
                depthEnd.displayTime = frameState.predictedDisplayTime;
                depthEnd.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
                depthEnd.layerCount = 1;
                depthEnd.layers = depthLayers;

                LARGE_INTEGER d0{}, d1{};
                QueryPerformanceCounter(&d0);
                const XrResult ended = xrEndFrame(m_impl->session, &depthEnd);
                QueryPerformanceCounter(&d1);
                if (XR_FAILED(ended)) {
                    error = std::format("xrEndFrame (with depth): {}",
                                        ResultString(m_impl->instance, ended));
                    return false;
                }
                result_.timings.endFrameMs = MillisecondsBetween(d0, d1);

                ++m_framesSubmitted;
                m_stage = Stage::FrameSubmitted;
                return true;
            }
        }
    }

    const XrCompositionLayerBaseHeader* layers[] = {
        reinterpret_cast<const XrCompositionLayerBaseHeader*>(&layer)};

    XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
    endInfo.displayTime = frameState.predictedDisplayTime;
    endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    endInfo.layerCount = submittedLayer ? 1u : 0u;
    endInfo.layers = submittedLayer ? layers : nullptr;

    LARGE_INTEGER e0{}, e1{};
    QueryPerformanceCounter(&e0);
    result = xrEndFrame(m_impl->session, &endInfo);
    QueryPerformanceCounter(&e1);
    if (XR_FAILED(result)) {
        error = std::format("xrEndFrame: {}", ResultString(m_impl->instance, result));
        return false;
    }
    result_.timings.endFrameMs = MillisecondsBetween(e0, e1);

    ++m_framesSubmitted;
    m_stage = Stage::FrameSubmitted;
    return true;
}

namespace {

static XrResult CreateActionHelper(
    XrActionSet set,
    XrActionType type,
    const char* name,
    const char* localizedName,
    uint32_t countSubactionPaths,
    const XrPath* subactionPaths,
    XrAction* outAction) {
    XrActionCreateInfo info{XR_TYPE_ACTION_CREATE_INFO};
    info.actionType = type;
    strncpy_s(info.actionName, sizeof(info.actionName), name, _TRUNCATE);
    strncpy_s(info.localizedActionName, sizeof(info.localizedActionName), localizedName, _TRUNCATE);
    info.countSubactionPaths = countSubactionPaths;
    info.subactionPaths = subactionPaths;
    return xrCreateAction(set, &info, outAction);
}

}  // namespace

bool SdkSession::InitializeInput(std::string& error) {
    if (m_impl == nullptr || m_impl->session == XR_NULL_HANDLE || m_impl->instance == XR_NULL_HANDLE) {
        error = "InitializeInput requires an active instance and session";
        return false;
    }

    if (m_impl->inputInitialized) {
        return true;
    }

    auto GetPath = [&](const char* pathStr) -> XrPath {
        XrPath p = XR_NULL_PATH;
        xrStringToPath(m_impl->instance, pathStr, &p);
        return p;
    };

    m_impl->handSubactionPaths[0] = GetPath("/user/hand/left");
    m_impl->handSubactionPaths[1] = GetPath("/user/hand/right");

    XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
    strncpy_s(actionSetInfo.actionSetName, sizeof(actionSetInfo.actionSetName), "nexvr_sdk_input", _TRUNCATE);
    strncpy_s(actionSetInfo.localizedActionSetName, sizeof(actionSetInfo.localizedActionSetName), "NexVR SDK Input", _TRUNCATE);
    actionSetInfo.priority = 0;

    XrResult res = xrCreateActionSet(m_impl->instance, &actionSetInfo, &m_impl->actionSet);
    if (XR_FAILED(res)) {
        error = std::format("xrCreateActionSet: {}", ResultString(m_impl->instance, res));
        return false;
    }

    // 6DOF Poses
    CreateActionHelper(m_impl->actionSet, XR_ACTION_TYPE_POSE_INPUT, "grip_pose", "Grip Pose",
                       2, m_impl->handSubactionPaths, &m_impl->actionGripPose);
    CreateActionHelper(m_impl->actionSet, XR_ACTION_TYPE_POSE_INPUT, "aim_pose", "Aim Pose",
                       2, m_impl->handSubactionPaths, &m_impl->actionAimPose);

    // Analog triggers, squeezes, and thumbsticks
    CreateActionHelper(m_impl->actionSet, XR_ACTION_TYPE_FLOAT_INPUT, "trigger_value", "Trigger Value",
                       2, m_impl->handSubactionPaths, &m_impl->actionTriggerValue);
    CreateActionHelper(m_impl->actionSet, XR_ACTION_TYPE_FLOAT_INPUT, "squeeze_value", "Squeeze Value",
                       2, m_impl->handSubactionPaths, &m_impl->actionSqueezeValue);
    CreateActionHelper(m_impl->actionSet, XR_ACTION_TYPE_VECTOR2F_INPUT, "thumbstick_xy", "Thumbstick XY",
                       2, m_impl->handSubactionPaths, &m_impl->actionThumbstickXY);

    // Digital Buttons
    CreateActionHelper(m_impl->actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "primary_click", "Primary Click",
                       2, m_impl->handSubactionPaths, &m_impl->actionPrimaryClick);
    CreateActionHelper(m_impl->actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "secondary_click", "Secondary Click",
                       2, m_impl->handSubactionPaths, &m_impl->actionSecondaryClick);
    CreateActionHelper(m_impl->actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "thumbstick_click", "Thumbstick Click",
                       2, m_impl->handSubactionPaths, &m_impl->actionThumbstickClick);
    CreateActionHelper(m_impl->actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "menu_click", "Menu Click",
                       2, m_impl->handSubactionPaths, &m_impl->actionMenuClick);
    CreateActionHelper(m_impl->actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "trigger_click", "Trigger Click",
                       2, m_impl->handSubactionPaths, &m_impl->actionTriggerClick);
    CreateActionHelper(m_impl->actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "squeeze_click", "Squeeze Click",
                       2, m_impl->handSubactionPaths, &m_impl->actionSqueezeClick);

    // Capacitive Touch Sensors
    CreateActionHelper(m_impl->actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "primary_touch", "Primary Touch",
                       2, m_impl->handSubactionPaths, &m_impl->actionPrimaryTouch);
    CreateActionHelper(m_impl->actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "secondary_touch", "Secondary Touch",
                       2, m_impl->handSubactionPaths, &m_impl->actionSecondaryTouch);
    CreateActionHelper(m_impl->actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "thumbstick_touch", "Thumbstick Touch",
                       2, m_impl->handSubactionPaths, &m_impl->actionThumbstickTouch);
    CreateActionHelper(m_impl->actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "trigger_touch", "Trigger Touch",
                       2, m_impl->handSubactionPaths, &m_impl->actionTriggerTouch);
    CreateActionHelper(m_impl->actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "thumbrest_touch", "Thumbrest Touch",
                       2, m_impl->handSubactionPaths, &m_impl->actionThumbrestTouch);

    // Haptics Output
    CreateActionHelper(m_impl->actionSet, XR_ACTION_TYPE_VIBRATION_OUTPUT, "vibrate", "Haptic Vibration",
                       2, m_impl->handSubactionPaths, &m_impl->actionVibrate);

    // Interaction profile 1: Oculus / Meta Touch
    std::vector<XrActionSuggestedBinding> oculusBindings = {
        {m_impl->actionGripPose, GetPath("/user/hand/left/input/grip/pose")},
        {m_impl->actionGripPose, GetPath("/user/hand/right/input/grip/pose")},
        {m_impl->actionAimPose, GetPath("/user/hand/left/input/aim/pose")},
        {m_impl->actionAimPose, GetPath("/user/hand/right/input/aim/pose")},
        {m_impl->actionTriggerValue, GetPath("/user/hand/left/input/trigger/value")},
        {m_impl->actionTriggerValue, GetPath("/user/hand/right/input/trigger/value")},
        {m_impl->actionSqueezeValue, GetPath("/user/hand/left/input/squeeze/value")},
        {m_impl->actionSqueezeValue, GetPath("/user/hand/right/input/squeeze/value")},
        {m_impl->actionThumbstickXY, GetPath("/user/hand/left/input/thumbstick")},
        {m_impl->actionThumbstickXY, GetPath("/user/hand/right/input/thumbstick")},
        {m_impl->actionThumbstickClick, GetPath("/user/hand/left/input/thumbstick/click")},
        {m_impl->actionThumbstickClick, GetPath("/user/hand/right/input/thumbstick/click")},
        {m_impl->actionPrimaryClick, GetPath("/user/hand/left/input/x/click")},
        {m_impl->actionPrimaryClick, GetPath("/user/hand/right/input/a/click")},
        {m_impl->actionSecondaryClick, GetPath("/user/hand/left/input/y/click")},
        {m_impl->actionSecondaryClick, GetPath("/user/hand/right/input/b/click")},
        {m_impl->actionMenuClick, GetPath("/user/hand/left/input/menu/click")},
        {m_impl->actionPrimaryTouch, GetPath("/user/hand/left/input/x/touch")},
        {m_impl->actionPrimaryTouch, GetPath("/user/hand/right/input/a/touch")},
        {m_impl->actionSecondaryTouch, GetPath("/user/hand/left/input/y/touch")},
        {m_impl->actionSecondaryTouch, GetPath("/user/hand/right/input/b/touch")},
        {m_impl->actionThumbstickTouch, GetPath("/user/hand/left/input/thumbstick/touch")},
        {m_impl->actionThumbstickTouch, GetPath("/user/hand/right/input/thumbstick/touch")},
        {m_impl->actionTriggerTouch, GetPath("/user/hand/left/input/trigger/touch")},
        {m_impl->actionTriggerTouch, GetPath("/user/hand/right/input/trigger/touch")},
        {m_impl->actionThumbrestTouch, GetPath("/user/hand/left/input/thumbrest/touch")},
        {m_impl->actionThumbrestTouch, GetPath("/user/hand/right/input/thumbrest/touch")},
        {m_impl->actionVibrate, GetPath("/user/hand/left/output/haptic")},
        {m_impl->actionVibrate, GetPath("/user/hand/right/output/haptic")},
    };
    XrInteractionProfileSuggestedBinding oculusSuggested{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    oculusSuggested.interactionProfile = GetPath("/interaction_profiles/oculus/touch_controller");
    oculusSuggested.countSuggestedBindings = static_cast<uint32_t>(oculusBindings.size());
    oculusSuggested.suggestedBindings = oculusBindings.data();
    xrSuggestInteractionProfileBindings(m_impl->instance, &oculusSuggested);

    // Interaction profile 2: Valve Index
    std::vector<XrActionSuggestedBinding> indexBindings = {
        {m_impl->actionGripPose, GetPath("/user/hand/left/input/grip/pose")},
        {m_impl->actionGripPose, GetPath("/user/hand/right/input/grip/pose")},
        {m_impl->actionAimPose, GetPath("/user/hand/left/input/aim/pose")},
        {m_impl->actionAimPose, GetPath("/user/hand/right/input/aim/pose")},
        {m_impl->actionTriggerValue, GetPath("/user/hand/left/input/trigger/value")},
        {m_impl->actionTriggerValue, GetPath("/user/hand/right/input/trigger/value")},
        {m_impl->actionTriggerClick, GetPath("/user/hand/left/input/trigger/click")},
        {m_impl->actionTriggerClick, GetPath("/user/hand/right/input/trigger/click")},
        {m_impl->actionSqueezeValue, GetPath("/user/hand/left/input/squeeze/value")},
        {m_impl->actionSqueezeValue, GetPath("/user/hand/right/input/squeeze/value")},
        {m_impl->actionThumbstickXY, GetPath("/user/hand/left/input/thumbstick")},
        {m_impl->actionThumbstickXY, GetPath("/user/hand/right/input/thumbstick")},
        {m_impl->actionThumbstickClick, GetPath("/user/hand/left/input/thumbstick/click")},
        {m_impl->actionThumbstickClick, GetPath("/user/hand/right/input/thumbstick/click")},
        {m_impl->actionPrimaryClick, GetPath("/user/hand/left/input/a/click")},
        {m_impl->actionPrimaryClick, GetPath("/user/hand/right/input/a/click")},
        {m_impl->actionSecondaryClick, GetPath("/user/hand/left/input/b/click")},
        {m_impl->actionSecondaryClick, GetPath("/user/hand/right/input/b/click")},
        {m_impl->actionPrimaryTouch, GetPath("/user/hand/left/input/a/touch")},
        {m_impl->actionPrimaryTouch, GetPath("/user/hand/right/input/a/touch")},
        {m_impl->actionSecondaryTouch, GetPath("/user/hand/left/input/b/touch")},
        {m_impl->actionSecondaryTouch, GetPath("/user/hand/right/input/b/touch")},
        {m_impl->actionThumbstickTouch, GetPath("/user/hand/left/input/thumbstick/touch")},
        {m_impl->actionThumbstickTouch, GetPath("/user/hand/right/input/thumbstick/touch")},
        {m_impl->actionTriggerTouch, GetPath("/user/hand/left/input/trigger/touch")},
        {m_impl->actionTriggerTouch, GetPath("/user/hand/right/input/trigger/touch")},
        {m_impl->actionVibrate, GetPath("/user/hand/left/output/haptic")},
        {m_impl->actionVibrate, GetPath("/user/hand/right/output/haptic")},
    };
    XrInteractionProfileSuggestedBinding indexSuggested{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    indexSuggested.interactionProfile = GetPath("/interaction_profiles/valve/index_controller");
    indexSuggested.countSuggestedBindings = static_cast<uint32_t>(indexBindings.size());
    indexSuggested.suggestedBindings = indexBindings.data();
    xrSuggestInteractionProfileBindings(m_impl->instance, &indexSuggested);

    // Interaction profile 3: HTC Vive
    std::vector<XrActionSuggestedBinding> viveBindings = {
        {m_impl->actionGripPose, GetPath("/user/hand/left/input/grip/pose")},
        {m_impl->actionGripPose, GetPath("/user/hand/right/input/grip/pose")},
        {m_impl->actionAimPose, GetPath("/user/hand/left/input/aim/pose")},
        {m_impl->actionAimPose, GetPath("/user/hand/right/input/aim/pose")},
        {m_impl->actionTriggerValue, GetPath("/user/hand/left/input/trigger/value")},
        {m_impl->actionTriggerValue, GetPath("/user/hand/right/input/trigger/value")},
        {m_impl->actionTriggerClick, GetPath("/user/hand/left/input/trigger/click")},
        {m_impl->actionTriggerClick, GetPath("/user/hand/right/input/trigger/click")},
        {m_impl->actionSqueezeClick, GetPath("/user/hand/left/input/squeeze/click")},
        {m_impl->actionSqueezeClick, GetPath("/user/hand/right/input/squeeze/click")},
        {m_impl->actionMenuClick, GetPath("/user/hand/left/input/menu/click")},
        {m_impl->actionMenuClick, GetPath("/user/hand/right/input/menu/click")},
        {m_impl->actionThumbstickXY, GetPath("/user/hand/left/input/trackpad")},
        {m_impl->actionThumbstickXY, GetPath("/user/hand/right/input/trackpad")},
        {m_impl->actionThumbstickClick, GetPath("/user/hand/left/input/trackpad/click")},
        {m_impl->actionThumbstickClick, GetPath("/user/hand/right/input/trackpad/click")},
        {m_impl->actionThumbstickTouch, GetPath("/user/hand/left/input/trackpad/touch")},
        {m_impl->actionThumbstickTouch, GetPath("/user/hand/right/input/trackpad/touch")},
        {m_impl->actionVibrate, GetPath("/user/hand/left/output/haptic")},
        {m_impl->actionVibrate, GetPath("/user/hand/right/output/haptic")},
    };
    XrInteractionProfileSuggestedBinding viveSuggested{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    viveSuggested.interactionProfile = GetPath("/interaction_profiles/htc/vive_controller");
    viveSuggested.countSuggestedBindings = static_cast<uint32_t>(viveBindings.size());
    viveSuggested.suggestedBindings = viveBindings.data();
    xrSuggestInteractionProfileBindings(m_impl->instance, &viveSuggested);

    // Interaction profile 4: Microsoft Mixed Reality
    std::vector<XrActionSuggestedBinding> mrBindings = {
        {m_impl->actionGripPose, GetPath("/user/hand/left/input/grip/pose")},
        {m_impl->actionGripPose, GetPath("/user/hand/right/input/grip/pose")},
        {m_impl->actionAimPose, GetPath("/user/hand/left/input/aim/pose")},
        {m_impl->actionAimPose, GetPath("/user/hand/right/input/aim/pose")},
        {m_impl->actionTriggerValue, GetPath("/user/hand/left/input/trigger/value")},
        {m_impl->actionTriggerValue, GetPath("/user/hand/right/input/trigger/value")},
        {m_impl->actionSqueezeClick, GetPath("/user/hand/left/input/squeeze/click")},
        {m_impl->actionSqueezeClick, GetPath("/user/hand/right/input/squeeze/click")},
        {m_impl->actionMenuClick, GetPath("/user/hand/left/input/menu/click")},
        {m_impl->actionThumbstickXY, GetPath("/user/hand/left/input/thumbstick")},
        {m_impl->actionThumbstickXY, GetPath("/user/hand/right/input/thumbstick")},
        {m_impl->actionThumbstickClick, GetPath("/user/hand/left/input/thumbstick/click")},
        {m_impl->actionThumbstickClick, GetPath("/user/hand/right/input/thumbstick/click")},
        {m_impl->actionVibrate, GetPath("/user/hand/left/output/haptic")},
        {m_impl->actionVibrate, GetPath("/user/hand/right/output/haptic")},
    };
    XrInteractionProfileSuggestedBinding mrSuggested{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    mrSuggested.interactionProfile = GetPath("/interaction_profiles/microsoft/motion_controller");
    mrSuggested.countSuggestedBindings = static_cast<uint32_t>(mrBindings.size());
    mrSuggested.suggestedBindings = mrBindings.data();
    xrSuggestInteractionProfileBindings(m_impl->instance, &mrSuggested);

    // Interaction profile 5: Simple Controller
    std::vector<XrActionSuggestedBinding> simpleBindings = {
        {m_impl->actionGripPose, GetPath("/user/hand/left/input/grip/pose")},
        {m_impl->actionGripPose, GetPath("/user/hand/right/input/grip/pose")},
        {m_impl->actionAimPose, GetPath("/user/hand/left/input/aim/pose")},
        {m_impl->actionAimPose, GetPath("/user/hand/right/input/aim/pose")},
        {m_impl->actionPrimaryClick, GetPath("/user/hand/left/input/select/click")},
        {m_impl->actionPrimaryClick, GetPath("/user/hand/right/input/select/click")},
        {m_impl->actionMenuClick, GetPath("/user/hand/left/input/menu/click")},
        {m_impl->actionMenuClick, GetPath("/user/hand/right/input/menu/click")},
        {m_impl->actionVibrate, GetPath("/user/hand/left/output/haptic")},
        {m_impl->actionVibrate, GetPath("/user/hand/right/output/haptic")},
    };
    XrInteractionProfileSuggestedBinding simpleSuggested{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    simpleSuggested.interactionProfile = GetPath("/interaction_profiles/khr/simple_controller");
    simpleSuggested.countSuggestedBindings = static_cast<uint32_t>(simpleBindings.size());
    simpleSuggested.suggestedBindings = simpleBindings.data();
    xrSuggestInteractionProfileBindings(m_impl->instance, &simpleSuggested);

    // Attach Action Set to Session
    XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attachInfo.countActionSets = 1;
    attachInfo.actionSets = &m_impl->actionSet;
    res = xrAttachSessionActionSets(m_impl->session, &attachInfo);
    if (XR_FAILED(res)) {
        error = std::format("xrAttachSessionActionSets: {}", ResultString(m_impl->instance, res));
        return false;
    }

    // Create Action Spaces for Left & Right Grip and Aim
    for (int h = 0; h < 2; ++h) {
        XrActionSpaceCreateInfo gripSpaceInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        gripSpaceInfo.action = m_impl->actionGripPose;
        gripSpaceInfo.subactionPath = m_impl->handSubactionPaths[h];
        gripSpaceInfo.poseInActionSpace.orientation.w = 1.0f;
        res = xrCreateActionSpace(m_impl->session, &gripSpaceInfo, &m_impl->gripSpaces[h]);
        if (XR_FAILED(res)) {
            error = std::format("xrCreateActionSpace(grip): {}", ResultString(m_impl->instance, res));
            return false;
        }

        XrActionSpaceCreateInfo aimSpaceInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        aimSpaceInfo.action = m_impl->actionAimPose;
        aimSpaceInfo.subactionPath = m_impl->handSubactionPaths[h];
        aimSpaceInfo.poseInActionSpace.orientation.w = 1.0f;
        res = xrCreateActionSpace(m_impl->session, &aimSpaceInfo, &m_impl->aimSpaces[h]);
        if (XR_FAILED(res)) {
            error = std::format("xrCreateActionSpace(aim): {}", ResultString(m_impl->instance, res));
            return false;
        }
    }

    m_impl->inputInitialized = true;
    return true;
}

bool SdkSession::SyncInput(std::string& error) {
    if (m_impl == nullptr || m_impl->session == XR_NULL_HANDLE || !m_impl->inputInitialized) {
        error = "Input is not initialized or session is not active";
        return false;
    }

    XrActiveActionSet activeSet{m_impl->actionSet, XR_NULL_PATH};
    XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
    syncInfo.countActiveActionSets = 1;
    syncInfo.activeActionSets = &activeSet;

    XrResult res = xrSyncActions(m_impl->session, &syncInfo);
    if (XR_FAILED(res)) {
        error = std::format("xrSyncActions: {}", ResultString(m_impl->instance, res));
        return false;
    }
    return true;
}

bool SdkSession::GetControllerState(NexVR_Hand hand, NexVR_ControllerState& outState, std::string& error) {
    if (m_impl == nullptr || m_impl->session == XR_NULL_HANDLE || !m_impl->inputInitialized) {
        error = "Input is not initialized or session is not active";
        return false;
    }

    const int h = static_cast<int>(hand);
    if (h < 0 || h >= 2) {
        error = "Invalid hand index";
        return false;
    }

    outState.isConnected = 0;
    outState.gripPoseValid = 0;
    outState.aimPoseValid = 0;
    outState.trigger = 0.0f;
    outState.grip = 0.0f;
    outState.thumbstickX = 0.0f;
    outState.thumbstickY = 0.0f;
    outState.buttonsDown = 0;
    outState.buttonsTouched = 0;
    std::memset(&outState.gripPose, 0, sizeof(NexVR_Pose));
    std::memset(&outState.aimPose, 0, sizeof(NexVR_Pose));
    outState.gripPose.orientation[3] = 1.0f;
    outState.aimPose.orientation[3] = 1.0f;
    std::memset(outState.linearVelocity, 0, sizeof(outState.linearVelocity));
    std::memset(outState.angularVelocity, 0, sizeof(outState.angularVelocity));

    XrPath subaction = m_impl->handSubactionPaths[h];
    XrTime time = m_impl->lastPredictedDisplayTime > 0 ? m_impl->lastPredictedDisplayTime : 1000;

    // Check grip pose action state
    XrActionStateGetInfo getGripInfo{XR_TYPE_ACTION_STATE_GET_INFO};
    getGripInfo.action = m_impl->actionGripPose;
    getGripInfo.subactionPath = subaction;
    XrActionStatePose gripState{XR_TYPE_ACTION_STATE_POSE};
    if (XR_SUCCEEDED(xrGetActionStatePose(m_impl->session, &getGripInfo, &gripState))) {
        if (gripState.isActive == XR_TRUE) {
            outState.isConnected = 1;
        }
    }

    // Locate grip space
    if (m_impl->gripSpaces[h] != XR_NULL_HANDLE && m_impl->space != XR_NULL_HANDLE) {
        XrSpaceVelocity velocity{XR_TYPE_SPACE_VELOCITY};
        XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
        location.next = &velocity;

        if (XR_SUCCEEDED(xrLocateSpace(m_impl->gripSpaces[h], m_impl->space, time, &location))) {
            constexpr XrSpaceLocationFlags kPoseValidFlags =
                XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
            if ((location.locationFlags & kPoseValidFlags) == kPoseValidFlags) {
                outState.gripPoseValid = 1;
                outState.isConnected = 1;
                outState.gripPose.orientation[0] = location.pose.orientation.x;
                outState.gripPose.orientation[1] = location.pose.orientation.y;
                outState.gripPose.orientation[2] = location.pose.orientation.z;
                outState.gripPose.orientation[3] = location.pose.orientation.w;
                outState.gripPose.position[0] = location.pose.position.x;
                outState.gripPose.position[1] = location.pose.position.y;
                outState.gripPose.position[2] = location.pose.position.z;
            }
            if (velocity.velocityFlags & XR_SPACE_VELOCITY_LINEAR_VALID_BIT) {
                outState.linearVelocity[0] = velocity.linearVelocity.x;
                outState.linearVelocity[1] = velocity.linearVelocity.y;
                outState.linearVelocity[2] = velocity.linearVelocity.z;
            }
            if (velocity.velocityFlags & XR_SPACE_VELOCITY_ANGULAR_VALID_BIT) {
                outState.angularVelocity[0] = velocity.angularVelocity.x;
                outState.angularVelocity[1] = velocity.angularVelocity.y;
                outState.angularVelocity[2] = velocity.angularVelocity.z;
            }
        }
    }

    // Locate aim space
    if (m_impl->aimSpaces[h] != XR_NULL_HANDLE && m_impl->space != XR_NULL_HANDLE) {
        XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
        if (XR_SUCCEEDED(xrLocateSpace(m_impl->aimSpaces[h], m_impl->space, time, &location))) {
            constexpr XrSpaceLocationFlags kPoseValidFlags =
                XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
            if ((location.locationFlags & kPoseValidFlags) == kPoseValidFlags) {
                outState.aimPoseValid = 1;
                outState.isConnected = 1;
                outState.aimPose.orientation[0] = location.pose.orientation.x;
                outState.aimPose.orientation[1] = location.pose.orientation.y;
                outState.aimPose.orientation[2] = location.pose.orientation.z;
                outState.aimPose.orientation[3] = location.pose.orientation.w;
                outState.aimPose.position[0] = location.pose.position.x;
                outState.aimPose.position[1] = location.pose.position.y;
                outState.aimPose.position[2] = location.pose.position.z;
            }
        }
    }

    // Analog triggers, squeezes, and sticks
    auto ReadFloat = [&](XrAction action) -> float {
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = action;
        getInfo.subactionPath = subaction;
        XrActionStateFloat state{XR_TYPE_ACTION_STATE_FLOAT};
        if (XR_SUCCEEDED(xrGetActionStateFloat(m_impl->session, &getInfo, &state)) && state.isActive) {
            outState.isConnected = 1;
            return state.currentState;
        }
        return 0.0f;
    };

    auto ReadBool = [&](XrAction action) -> bool {
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = action;
        getInfo.subactionPath = subaction;
        XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
        if (XR_SUCCEEDED(xrGetActionStateBoolean(m_impl->session, &getInfo, &state)) && state.isActive) {
            outState.isConnected = 1;
            return state.currentState == XR_TRUE;
        }
        return false;
    };

    outState.trigger = ReadFloat(m_impl->actionTriggerValue);
    outState.grip = ReadFloat(m_impl->actionSqueezeValue);

    XrActionStateGetInfo getStickInfo{XR_TYPE_ACTION_STATE_GET_INFO};
    getStickInfo.action = m_impl->actionThumbstickXY;
    getStickInfo.subactionPath = subaction;
    XrActionStateVector2f stickState{XR_TYPE_ACTION_STATE_VECTOR2F};
    if (XR_SUCCEEDED(xrGetActionStateVector2f(m_impl->session, &getStickInfo, &stickState)) && stickState.isActive) {
        outState.isConnected = 1;
        outState.thumbstickX = stickState.currentState.x;
        outState.thumbstickY = stickState.currentState.y;
    }

    if (ReadBool(m_impl->actionPrimaryClick)) outState.buttonsDown |= NEXVR_BUTTON_PRIMARY;
    if (ReadBool(m_impl->actionSecondaryClick)) outState.buttonsDown |= NEXVR_BUTTON_SECONDARY;
    if (ReadBool(m_impl->actionThumbstickClick)) outState.buttonsDown |= NEXVR_BUTTON_THUMBSTICK;
    if (ReadBool(m_impl->actionMenuClick)) outState.buttonsDown |= NEXVR_BUTTON_MENU;
    if (ReadBool(m_impl->actionTriggerClick) || outState.trigger > 0.85f) outState.buttonsDown |= NEXVR_BUTTON_TRIGGER_CLICK;
    if (ReadBool(m_impl->actionSqueezeClick) || outState.grip > 0.85f) outState.buttonsDown |= NEXVR_BUTTON_GRIP_CLICK;

    if (ReadBool(m_impl->actionPrimaryTouch)) outState.buttonsTouched |= NEXVR_TOUCH_PRIMARY;
    if (ReadBool(m_impl->actionSecondaryTouch)) outState.buttonsTouched |= NEXVR_TOUCH_SECONDARY;
    if (ReadBool(m_impl->actionThumbstickTouch)) outState.buttonsTouched |= NEXVR_TOUCH_THUMBSTICK;
    if (ReadBool(m_impl->actionTriggerTouch) || outState.trigger > 0.05f) outState.buttonsTouched |= NEXVR_TOUCH_TRIGGER;
    if (ReadBool(m_impl->actionThumbrestTouch)) outState.buttonsTouched |= NEXVR_TOUCH_THUMBREST;

    return true;
}

bool SdkSession::TriggerHaptic(NexVR_Hand hand, const NexVR_HapticFeedback& haptic, std::string& error) {
    if (m_impl == nullptr || m_impl->session == XR_NULL_HANDLE || !m_impl->inputInitialized) {
        error = "Input is not initialized or session is not active";
        return false;
    }

    const int h = static_cast<int>(hand);
    if (h < 0 || h >= 2) {
        error = "Invalid hand index";
        return false;
    }

    XrHapticActionInfo actionInfo{XR_TYPE_HAPTIC_ACTION_INFO};
    actionInfo.action = m_impl->actionVibrate;
    actionInfo.subactionPath = m_impl->handSubactionPaths[h];

    if (haptic.durationMs <= 0.0f || haptic.amplitude <= 0.0f) {
        XrResult res = xrStopHapticFeedback(m_impl->session, &actionInfo);
        if (XR_FAILED(res)) {
            error = std::format("xrStopHapticFeedback: {}", ResultString(m_impl->instance, res));
            return false;
        }
        return true;
    }

    XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
    vibration.duration = static_cast<XrDuration>(haptic.durationMs * 1'000'000.0f);
    vibration.frequency = haptic.frequencyHz > 0.0f ? haptic.frequencyHz : XR_FREQUENCY_UNSPECIFIED;
    vibration.amplitude = haptic.amplitude > 1.0f ? 1.0f : (haptic.amplitude < 0.0f ? 0.0f : haptic.amplitude);

    XrResult res = xrApplyHapticFeedback(
        m_impl->session, &actionInfo, reinterpret_cast<const XrHapticBaseHeader*>(&vibration));
    if (XR_FAILED(res)) {
        error = std::format("xrApplyHapticFeedback: {}", ResultString(m_impl->instance, res));
        return false;
    }
    return true;
}

bool SdkSession::StopHaptic(NexVR_Hand hand, std::string& error) {
    if (m_impl == nullptr || m_impl->session == XR_NULL_HANDLE || !m_impl->inputInitialized) {
        error = "Input is not initialized or session is not active";
        return false;
    }

    const int h = static_cast<int>(hand);
    if (h < 0 || h >= 2) {
        error = "Invalid hand index";
        return false;
    }

    XrHapticActionInfo actionInfo{XR_TYPE_HAPTIC_ACTION_INFO};
    actionInfo.action = m_impl->actionVibrate;
    actionInfo.subactionPath = m_impl->handSubactionPaths[h];

    XrResult res = xrStopHapticFeedback(m_impl->session, &actionInfo);
    if (XR_FAILED(res)) {
        error = std::format("xrStopHapticFeedback: {}", ResultString(m_impl->instance, res));
        return false;
    }
    return true;
}

void SdkSession::Shutdown() {
    if (m_impl == nullptr) {
        return;
    }

    for (int h = 0; h < 2; ++h) {
        if (m_impl->gripSpaces[h] != XR_NULL_HANDLE) {
            xrDestroySpace(m_impl->gripSpaces[h]);
            m_impl->gripSpaces[h] = XR_NULL_HANDLE;
        }
        if (m_impl->aimSpaces[h] != XR_NULL_HANDLE) {
            xrDestroySpace(m_impl->aimSpaces[h]);
            m_impl->aimSpaces[h] = XR_NULL_HANDLE;
        }
    }
    if (m_impl->actionSet != XR_NULL_HANDLE) {
        xrDestroyActionSet(m_impl->actionSet);
        m_impl->actionSet = XR_NULL_HANDLE;
    }
    m_impl->inputInitialized = false;

    if (m_impl->swapchain != XR_NULL_HANDLE) {
        xrDestroySwapchain(m_impl->swapchain);
        m_impl->swapchain = XR_NULL_HANDLE;
    }
    if (m_impl->depthSwapchain != XR_NULL_HANDLE) {
        xrDestroySwapchain(m_impl->depthSwapchain);
        m_impl->depthSwapchain = XR_NULL_HANDLE;
    }
    m_swapchainImages.clear();
    m_depthSwapchainImages.clear();
    m_d3d12SwapchainImages.clear();
    m_d3d12DepthSwapchainImages.clear();

    if (m_impl->space != XR_NULL_HANDLE) {
        xrDestroySpace(m_impl->space);
        m_impl->space = XR_NULL_HANDLE;
    }
    if (m_impl->session != XR_NULL_HANDLE) {
        xrDestroySession(m_impl->session);
        m_impl->session = XR_NULL_HANDLE;
    }
    if (m_impl->instance != XR_NULL_HANDLE) {
        xrDestroyInstance(m_impl->instance);
        m_impl->instance = XR_NULL_HANDLE;
    }

    if (m_d3d12Fence && m_d3d12FenceValue > 0) {
        if (m_d3d12Fence->GetCompletedValue() < m_d3d12FenceValue && m_d3d12FenceEvent) {
            m_d3d12Fence->SetEventOnCompletion(m_d3d12FenceValue, m_d3d12FenceEvent);
            WaitForSingleObject(m_d3d12FenceEvent, 2000);
        }
    }

    if (m_d3d12FenceEvent != nullptr) {
        CloseHandle(m_d3d12FenceEvent);
        m_d3d12FenceEvent = nullptr;
    }

    m_d3d12CommandList.Reset();
    m_d3d12CommandAllocator.Reset();
    m_d3d12Fence.Reset();
    m_d3d12Queue.Reset();
    m_d3d12Device.Reset();

    m_context.Reset();
    m_device.Reset();

    m_sessionRunning = false;
}

}  // namespace nexvr::sdk
