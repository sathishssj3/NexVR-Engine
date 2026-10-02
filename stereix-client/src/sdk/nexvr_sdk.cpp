// The C ABI, and deliberately almost nothing else.
//
// Every function here does three things: check its arguments, call into
// SdkSession, and translate the answer into a NexVR_Result / Stereix_Result.
//
// The boundary discipline: no exception may cross it, no C++ type may appear
// in a signature, and no pointer handed to us is trusted without being checked.
//
// All entry points are protected by the Stereix Zero-Crash SEH Shield,
// preventing host game process crashes even in the event of access violations,
// wild pointers, or unhandled graphics driver faults.

#include "nexvr_sdk.h"
#include "stereix_sdk.h"

#include "sdk_session.h"
#include "sdk_validation.h"

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

#include <cstdio>
#include <memory>
#include <new>
#include <string>

namespace {

using nexvr::sdk::SdkSession;

/**
 * The opaque handle's real type.
 *
 * Constraints captured at GetGraphicsRequirements are stored ALONGSIDE the
 * session rather than re-derived at init, because re-deriving them would ask
 * the runtime a second time and quietly tolerate a different answer. What the
 * game was told is what the game must be held to.
 */
struct SessionImpl {
    SdkSession core;
    nexvr::sdk::RuntimeConstraints constraints;
    bool verifyCopies{false};
};

/**
 * Last error detail, per thread.
 *
 * Thread-local because the SDK is explicitly a two-thread API: the simulation
 * thread and the render thread fail at different things at the same time, and
 * a shared buffer would hand each of them the other's explanation.
 */
thread_local std::string g_lastError;

void SetDetail(std::string detail) {
    g_lastError = std::move(detail);
}

/**
 * The process-wide constraints from the last GetGraphicsRequirements.
 *
 * A global, reluctantly, because the requirements query happens BEFORE any
 * session handle exists — that ordering is the entire point of the call, so
 * there is nowhere else to put the answer. Single-session by construction,
 * which matches the one-VR-session-per-game reality.
 */
nexvr::sdk::RuntimeConstraints g_constraints;

/** Reduce a texture to the facts a copy actually depends on. */
nexvr::sdk::TextureFacts DescribeTexture(ID3D11Texture2D* texture) {
    nexvr::sdk::TextureFacts facts;
    if (texture == nullptr) {
        return facts;
    }
    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    facts.width = desc.Width;
    facts.height = desc.Height;
    facts.format = desc.Format;
    facts.sampleCount = desc.SampleDesc.Count;
    facts.valid = true;
    return facts;
}

nexvr::sdk::TextureFacts DescribeTextureDX12(ID3D12Resource* texture) {
    nexvr::sdk::TextureFacts facts;
    if (texture == nullptr) {
        return facts;
    }
    const D3D12_RESOURCE_DESC desc = texture->GetDesc();
    facts.width = static_cast<uint32_t>(desc.Width);
    facts.height = desc.Height;
    facts.format = desc.Format;
    facts.sampleCount = desc.SampleDesc.Count;
    facts.valid = true;
    return facts;
}

// ===========================================================================
// ZERO-CRASH HARNESS (SEH + C++ Exception Isolation)
// ===========================================================================
// Enterprise B2B contract: an invalid pointer, driver fault, or unhandled
// exception within our runtime must NEVER crash the host game engine process.
//
// Under MSVC (/EHsc), __try cannot coexist in the same stack frame with C++
// objects that have destructors (compiler error C2712). We isolate __try/__except
// in CallWithSEH, a leaf trampoline accepting a plain function pointer and
// context pointer.

typedef void (*SEHCallback)(void* context);

#if defined(_MSC_VER)
__declspec(noinline) int CallWithSEH(SEHCallback callback, void* context, DWORD* outExceptionCode) {
    __try {
        callback(context);
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (outExceptionCode != nullptr) {
            *outExceptionCode = GetExceptionCode();
        }
        return -1;
    }
}
#else
inline int CallWithSEH(SEHCallback callback, void* context, DWORD* outExceptionCode) {
    callback(context);
    return 0;
}
#endif

template <typename F>
NexVR_Result SafeApiCall(F&& func) {
    NexVR_Result result = NEXVR_ERROR_RUNTIME_FAILURE;
    bool normalReturn = false;
    DWORD sehCode = 0;

    struct Context {
        F* fn;
        NexVR_Result* res;
        bool* normal;
    } ctx = { &func, &result, &normalReturn };

    auto invoker = [](void* raw) {
        auto* c = static_cast<Context*>(raw);
        try {
            *(c->res) = (*(c->fn))();
            *(c->normal) = true;
        } catch (const std::bad_alloc&) {
            SetDetail("Stereix Zero-Crash Shield: Out of memory in SDK runtime");
            *(c->res) = NEXVR_ERROR_RUNTIME_FAILURE;
        } catch (const std::exception& ex) {
            SetDetail(std::string("Stereix Zero-Crash Shield: Internal exception caught: ") + ex.what());
            *(c->res) = NEXVR_ERROR_RUNTIME_FAILURE;
        } catch (...) {
            SetDetail("Stereix Zero-Crash Shield: Unknown internal C++ exception caught");
            *(c->res) = NEXVR_ERROR_RUNTIME_FAILURE;
        }
    };

    int sehStatus = CallWithSEH(invoker, &ctx, &sehCode);
    if (sehStatus != 0 || !normalReturn) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "Stereix Zero-Crash Shield: Hardware/OS fault prevented (SEH code 0x%08lX)", sehCode);
        SetDetail(buf);
        return NEXVR_ERROR_RUNTIME_FAILURE;
    }

    return result;
}

template <typename F>
void SafeApiCallVoid(F&& func) {
    DWORD sehCode = 0;
    struct Context {
        F* fn;
    } ctx = { &func };

    auto invoker = [](void* raw) {
        auto* c = static_cast<Context*>(raw);
        try {
            (*(c->fn))();
        } catch (...) {
            SetDetail("Stereix Zero-Crash Shield: Exception caught in shutdown handler");
        }
    };

    int sehStatus = CallWithSEH(invoker, &ctx, &sehCode);
    if (sehStatus != 0) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "Stereix Zero-Crash Shield: Hardware/OS fault prevented in shutdown (SEH code 0x%08lX)", sehCode);
        SetDetail(buf);
    }
}

template <typename F>
uint32_t SafeApiCallUint32(F&& func) {
    uint32_t result = 0;
    bool normalReturn = false;
    DWORD sehCode = 0;

    struct Context {
        F* fn;
        uint32_t* res;
        bool* normal;
    } ctx = { &func, &result, &normalReturn };

    auto invoker = [](void* raw) {
        auto* c = static_cast<Context*>(raw);
        try {
            *(c->res) = (*(c->fn))();
            *(c->normal) = true;
        } catch (...) {
            *(c->res) = 0;
        }
    };

    int sehStatus = CallWithSEH(invoker, &ctx, &sehCode);
    if (sehStatus != 0 || !normalReturn) {
        return 0;
    }
    return result;
}

}  // namespace

extern "C" {

NEXVR_API NexVR_Result NEXVR_CALL
NexVR_GetGraphicsRequirements(NexVR_GraphicsRequirements* requirements) {
    return SafeApiCall([=]() -> NexVR_Result {
        if (requirements == nullptr || requirements->structSize < sizeof(NexVR_GraphicsRequirements)) {
            SetDetail("requirements was null, or structSize was not set to sizeof(NexVR_GraphicsRequirements)");
            return NEXVR_ERROR_INVALID_ARGUMENT;
        }

        // A throwaway session purely to ask the runtime. It creates an XrInstance
        // and destroys it again, which is wasteful and correct: holding one open
        // between this call and NexVR_InitializeDX11 would mean the SDK owned a
        // runtime connection during the entire span of the game's renderer
        // startup, for no benefit.
        SdkSession probe;
        nexvr::sdk::Requirements found{};
        std::string error;

        if (!probe.QueryRequirements(found, error)) {
            SetDetail(error);
            return probe.ReachedStage() >= nexvr::sdk::Stage::InstanceCreated ? NEXVR_ERROR_NO_HEADSET
                                                                             : NEXVR_ERROR_RUNTIME_UNAVAILABLE;
        }

        requirements->adapterLuid = found.adapterLuid;
        requirements->minFeatureLevel = 0xb000;  // D3D_FEATURE_LEVEL_11_0
        requirements->recommendedWidth = found.eyeWidth;
        requirements->recommendedHeight = found.eyeHeight;
        requirements->maxWidth = found.eyeWidth;
        requirements->maxHeight = found.eyeHeight;
        requirements->viewCount = found.viewCount;

        // Precomputed so an integrator cannot get the arithmetic wrong.
        requirements->requiredTextureWidth = found.eyeWidth * found.viewCount;
        requirements->requiredTextureHeight = found.eyeHeight;
        requirements->swapchainFormat = DXGI_FORMAT_UNKNOWN;

        g_constraints.adapterLuid = found.adapterLuid;
        g_constraints.requiredWidth = requirements->requiredTextureWidth;
        g_constraints.requiredHeight = requirements->requiredTextureHeight;
        g_constraints.swapchainFormat = DXGI_FORMAT_UNKNOWN;
        g_constraints.queried = true;

        SetDetail({});
        return NEXVR_SUCCESS;
    });
}

NEXVR_API NexVR_Result NEXVR_CALL
NexVR_GetGraphicsRequirementsDX12(NexVR_GraphicsRequirements* requirements) {
    return SafeApiCall([=]() -> NexVR_Result {
        if (requirements == nullptr || requirements->structSize < sizeof(NexVR_GraphicsRequirements)) {
            SetDetail("requirements was null, or structSize was not set to sizeof(NexVR_GraphicsRequirements)");
            return NEXVR_ERROR_INVALID_ARGUMENT;
        }

        SdkSession probe;
        nexvr::sdk::Requirements found{};
        std::string error;

        if (!probe.QueryRequirementsDX12(found, error)) {
            SetDetail(error);
            return probe.ReachedStage() >= nexvr::sdk::Stage::InstanceCreated ? NEXVR_ERROR_NO_HEADSET
                                                                             : NEXVR_ERROR_RUNTIME_UNAVAILABLE;
        }

        requirements->adapterLuid = found.adapterLuid;
        requirements->minFeatureLevel = 0xc000;  // D3D_FEATURE_LEVEL_12_0
        requirements->recommendedWidth = found.eyeWidth;
        requirements->recommendedHeight = found.eyeHeight;
        requirements->maxWidth = found.eyeWidth;
        requirements->maxHeight = found.eyeHeight;
        requirements->viewCount = found.viewCount;

        requirements->requiredTextureWidth = found.eyeWidth * found.viewCount;
        requirements->requiredTextureHeight = found.eyeHeight;
        requirements->swapchainFormat = DXGI_FORMAT_UNKNOWN;

        g_constraints.adapterLuid = found.adapterLuid;
        g_constraints.requiredWidth = requirements->requiredTextureWidth;
        g_constraints.requiredHeight = requirements->requiredTextureHeight;
        g_constraints.swapchainFormat = DXGI_FORMAT_UNKNOWN;
        g_constraints.queried = true;

        SetDetail({});
        return NEXVR_SUCCESS;
    });
}

NEXVR_API NexVR_Result NEXVR_CALL
NexVR_InitializeDX11(const NexVR_InitializeDX11Info* info, NexVR_Session* outSession) {
    return SafeApiCall([=]() -> NexVR_Result {
        if (info == nullptr || outSession == nullptr ||
            info->structSize < sizeof(NexVR_InitializeDX11Info)) {
            SetDetail("info or outSession was null, or structSize was not set");
            return NEXVR_ERROR_INVALID_ARGUMENT;
        }
        *outSession = nullptr;

        if (!g_constraints.queried) {
            SetDetail("GetGraphicsRequirements must be called successfully before InitializeDX11");
            return NEXVR_ERROR_REQUIREMENTS_NOT_QUERIED;
        }

        if (info->device == nullptr || info->immediateContext == nullptr) {
            SetDetail("device and immediateContext are both required");
            return NEXVR_ERROR_INVALID_ARGUMENT;
        }

        auto session = std::make_unique<SessionImpl>();
        session->constraints = g_constraints;
        session->verifyCopies = info->enableCopyVerification != 0;

        std::string error;
        nexvr::sdk::Requirements ignored{};
        if (!session->core.QueryRequirements(ignored, error)) {
            SetDetail(error);
            return NEXVR_ERROR_RUNTIME_UNAVAILABLE;
        }

        if (!session->core.BindDevice(info->device, info->immediateContext, error)) {
            SetDetail(error);
            return NEXVR_ERROR_INVALID_ARGUMENT;
        }

        LUID deviceLuid{};
        if (!session->core.DeviceAdapterLuid(deviceLuid)) {
            SetDetail("the D3D11 device does not expose a DXGI adapter");
            return NEXVR_ERROR_INVALID_ARGUMENT;
        }

        std::string detail;
        const NexVR_Result verdict = nexvr::sdk::ValidateInitialize(
            session->constraints, DescribeTexture(info->representativeTarget), info->colorSpace,
            deviceLuid, session->core.ContextIsImmediate(), session->core.MultithreadProtected(),
            detail);

        if (NEXVR_FAILED(verdict)) {
            SetDetail(std::move(detail));
            return verdict;
        }

        const nexvr::sdk::TextureFacts target = DescribeTexture(info->representativeTarget);
        if (!session->core.Initialize(info->device, target.format, error)) {
            SetDetail(error);
            return session->core.FailedOnFormat() ? NEXVR_ERROR_INVALID_FORMAT
                                                  : NEXVR_ERROR_RUNTIME_FAILURE;
        }

        if (!session->core.MultithreadWasAlreadyOn()) {
            SetDetail(
                "NexVR enabled ID3D11Multithread on this device. If the renderer relied on the "
                "immediate context being unsynchronised, this inserts a lock into its hot path.");
        } else {
            SetDetail({});
        }

        *outSession = reinterpret_cast<NexVR_Session>(session.release());
        return NEXVR_SUCCESS;
    });
}

NEXVR_API NexVR_Result NEXVR_CALL
NexVR_InitializeDX12(const NexVR_InitializeDX12Info* info, NexVR_Session* outSession) {
    return SafeApiCall([=]() -> NexVR_Result {
        if (info == nullptr || outSession == nullptr ||
            info->structSize < sizeof(NexVR_InitializeDX12Info)) {
            SetDetail("info or outSession was null, or structSize was not set");
            return NEXVR_ERROR_INVALID_ARGUMENT;
        }
        *outSession = nullptr;

        if (!g_constraints.queried) {
            SetDetail("GetGraphicsRequirementsDX12 must be called successfully before InitializeDX12");
            return NEXVR_ERROR_REQUIREMENTS_NOT_QUERIED;
        }

        if (info->device == nullptr || info->commandQueue == nullptr) {
            SetDetail("device and commandQueue are both required");
            return NEXVR_ERROR_INVALID_ARGUMENT;
        }

        auto session = std::make_unique<SessionImpl>();
        session->constraints = g_constraints;
        session->verifyCopies = info->enableCopyVerification != 0;

        std::string error;
        nexvr::sdk::Requirements ignored{};
        if (!session->core.QueryRequirementsDX12(ignored, error)) {
            SetDetail(error);
            return NEXVR_ERROR_RUNTIME_UNAVAILABLE;
        }

        if (!session->core.BindDeviceDX12(info->device, info->commandQueue, error)) {
            SetDetail(error);
            return NEXVR_ERROR_INVALID_ARGUMENT;
        }

        LUID deviceLuid{};
        if (!session->core.DeviceAdapterLuidDX12(deviceLuid)) {
            SetDetail("the D3D12 device did not report an adapter LUID");
            return NEXVR_ERROR_INVALID_ARGUMENT;
        }

        std::string detail;
        const NexVR_Result verdict = nexvr::sdk::ValidateInitializeDX12(
            session->constraints, DescribeTextureDX12(info->representativeTarget), info->colorSpace,
            deviceLuid, session->core.QueueIsDirect(), detail);

        if (NEXVR_FAILED(verdict)) {
            SetDetail(std::move(detail));
            return verdict;
        }

        const nexvr::sdk::TextureFacts target = DescribeTextureDX12(info->representativeTarget);
        if (!session->core.InitializeDX12(info->device, info->commandQueue, target.format, error)) {
            SetDetail(error);
            return session->core.FailedOnFormat() ? NEXVR_ERROR_INVALID_FORMAT
                                                  : NEXVR_ERROR_RUNTIME_FAILURE;
        }

        SetDetail({});
        *outSession = reinterpret_cast<NexVR_Session>(session.release());
        return NEXVR_SUCCESS;
    });
}

NEXVR_API NexVR_Result NEXVR_CALL
NexVR_WaitFrame(NexVR_Session session, NexVR_FrameState* outFrame) {
    return SafeApiCall([=]() -> NexVR_Result {
        auto* impl = reinterpret_cast<SessionImpl*>(session);
        if (impl == nullptr || outFrame == nullptr ||
            outFrame->structSize < sizeof(NexVR_FrameState)) {
            SetDetail("session was null, or outFrame was null or had no structSize");
            return NEXVR_ERROR_INVALID_ARGUMENT;
        }

        std::string error;
        if (!impl->core.PumpEvents(error)) {
            SetDetail(error.empty() ? "the runtime asked the session to exit" : error);
            return NEXVR_SESSION_EXITING;
        }
        if (!impl->core.ReadyToSubmit()) {
            outFrame->token = 0;
            outFrame->predictedDisplayTime = 0;
            return nexvr::sdk::PublishFrameState(false, false, 0, nullptr, *outFrame);
        }

        nexvr::sdk::FrameHandoff handoff{};
        if (!impl->core.WaitFrame(handoff, error)) {
            SetDetail(error);
            return NEXVR_ERROR_RUNTIME_FAILURE;
        }

        outFrame->token = handoff.token;
        outFrame->predictedDisplayTime = handoff.predictedDisplayTime;

        SetDetail({});

        return nexvr::sdk::PublishFrameState(handoff.shouldRender, handoff.posesValid,
                                             handoff.viewCount, handoff.views, *outFrame);
    });
}

NEXVR_API NexVR_Result NEXVR_CALL
NexVR_SubmitFrameDX11(NexVR_Session session, uint64_t token, ID3D11Texture2D* colorTexture) {
    return NexVR_SubmitFrameWithDepthDX11(session, token, colorTexture, nullptr);
}

NEXVR_API uint32_t NEXVR_CALL NexVR_SupportsDepthSubmission(NexVR_Session session) {
    return SafeApiCallUint32([=]() -> uint32_t {
        auto* impl = reinterpret_cast<SessionImpl*>(session);
        return (impl != nullptr && impl->core.DepthUsable()) ? 1u : 0u;
    });
}

NEXVR_API NexVR_Result NEXVR_CALL
NexVR_SubmitFrameWithDepthDX11(NexVR_Session session, uint64_t token,
                               ID3D11Texture2D* colorTexture, const NexVR_DepthInfoDX11* depth) {
    return SafeApiCall([=]() -> NexVR_Result {
        auto* impl = reinterpret_cast<SessionImpl*>(session);
        if (impl == nullptr) {
            SetDetail("session was null");
            return NEXVR_ERROR_INVALID_SESSION;
        }

        bool shouldRender = false;
        (void)impl->core.TokenShouldRender(token, shouldRender);

        std::string detail;
        const NexVR_Result admissible = nexvr::sdk::ValidateSubmit(
            shouldRender, DescribeTexture(colorTexture), impl->constraints, detail);
        if (NEXVR_FAILED(admissible)) {
            SetDetail(std::move(detail));
            return admissible;
        }

        nexvr::sdk::SdkSession::DepthSubmission submission{};
        const nexvr::sdk::SdkSession::DepthSubmission* depthPtr = nullptr;

        if (depth != nullptr && depth->depthTexture != nullptr && shouldRender &&
            impl->core.DepthUsable()) {
            std::string depthDetail;
            const NexVR_Result depthVerdict = nexvr::sdk::ValidateDepthSubmit(
                depth->structSize, DescribeTexture(colorTexture), DescribeTexture(depth->depthTexture),
                depth->nearZ, depth->farZ, depthDetail);
            if (NEXVR_FAILED(depthVerdict)) {
                SetDetail(std::move(depthDetail));
                return depthVerdict;
            }

            submission.texture = depth->depthTexture;
            submission.nearZ = depth->nearZ;
            submission.farZ = depth->farZ;
            submission.reversed = depth->range == NEXVR_DEPTH_RANGE_REVERSED;
            depthPtr = &submission;
        }

        nexvr::sdk::FrameResult result{};
        std::string error;
        if (!impl->core.SubmitFrameThreaded(token, colorTexture, impl->verifyCopies, result, error,
                                            depthPtr)) {
            SetDetail(error);
            return error.find("token") != std::string::npos ? NEXVR_ERROR_INVALID_FRAME_TOKEN
                                                            : NEXVR_ERROR_RUNTIME_FAILURE;
        }

        if (result.droppedOnTimeout) {
            SetDetail("the compositor did not free a swapchain image within the deadline; frame dropped");
            return NEXVR_ERROR_SWAPCHAIN_TIMEOUT;
        }

        if (result.verifyAttempted && !result.copyVerified) {
            SetDetail("copy verification could not read the swapchain image back");
            return NEXVR_ERROR_RUNTIME_FAILURE;
        }

        SetDetail({});
        return NEXVR_SUCCESS;
    });
}

NEXVR_API NexVR_Result NEXVR_CALL
NexVR_SubmitFrameDX12(NexVR_Session session, uint64_t token,
                      ID3D12Resource* colorTexture,
                      D3D12_RESOURCE_STATES colorState) {
    return NexVR_SubmitFrameWithDepthDX12(session, token, colorTexture, colorState, nullptr);
}

NEXVR_API NexVR_Result NEXVR_CALL
NexVR_SubmitFrameWithDepthDX12(NexVR_Session session, uint64_t token,
                               ID3D12Resource* colorTexture,
                               D3D12_RESOURCE_STATES colorState,
                               const NexVR_DepthInfoDX12* depth) {
    return SafeApiCall([=]() -> NexVR_Result {
        auto* impl = reinterpret_cast<SessionImpl*>(session);
        if (impl == nullptr) {
            SetDetail("session was null");
            return NEXVR_ERROR_INVALID_SESSION;
        }

        bool shouldRender = false;
        (void)impl->core.TokenShouldRender(token, shouldRender);

        std::string detail;
        const NexVR_Result admissible = nexvr::sdk::ValidateSubmit(
            shouldRender, DescribeTextureDX12(colorTexture), impl->constraints, detail);
        if (NEXVR_FAILED(admissible)) {
            SetDetail(std::move(detail));
            return admissible;
        }

        nexvr::sdk::SdkSession::DepthSubmissionDX12 submission{};
        const nexvr::sdk::SdkSession::DepthSubmissionDX12* depthPtr = nullptr;

        if (depth != nullptr && depth->depthTexture != nullptr && shouldRender &&
            impl->core.DepthUsable()) {
            std::string depthDetail;
            const NexVR_Result depthVerdict = nexvr::sdk::ValidateDepthSubmitDX12(
                depth->structSize, DescribeTextureDX12(colorTexture), DescribeTextureDX12(depth->depthTexture),
                depth->nearZ, depth->farZ, depthDetail);
            if (NEXVR_FAILED(depthVerdict)) {
                SetDetail(std::move(depthDetail));
                return depthVerdict;
            }

            submission.texture = depth->depthTexture;
            submission.nearZ = depth->nearZ;
            submission.farZ = depth->farZ;
            submission.reversed = depth->range == NEXVR_DEPTH_RANGE_REVERSED;
            submission.state = depth->depthState;
            depthPtr = &submission;
        }

        nexvr::sdk::FrameResult result{};
        std::string error;
        if (!impl->core.SubmitFrameThreadedDX12(token, colorTexture, colorState, impl->verifyCopies,
                                                result, error, depthPtr)) {
            SetDetail(error);
            return error.find("token") != std::string::npos ? NEXVR_ERROR_INVALID_FRAME_TOKEN
                                                            : NEXVR_ERROR_RUNTIME_FAILURE;
        }

        if (result.droppedOnTimeout) {
            SetDetail("the compositor did not free a swapchain image within the deadline; frame dropped");
            return NEXVR_ERROR_SWAPCHAIN_TIMEOUT;
        }

        if (result.verifyAttempted && !result.copyVerified) {
            SetDetail("copy verification could not read the swapchain image back");
            return NEXVR_ERROR_RUNTIME_FAILURE;
        }

        SetDetail({});
        return NEXVR_SUCCESS;
    });
}

NEXVR_API NexVR_Result NEXVR_CALL NexVR_SyncInput(NexVR_Session session) {
    return SafeApiCall([=]() -> NexVR_Result {
        auto* impl = reinterpret_cast<SessionImpl*>(session);
        if (impl == nullptr) {
            SetDetail("session was null");
            return NEXVR_ERROR_INVALID_SESSION;
        }

        std::string error;
        if (!impl->core.SyncInput(error)) {
            SetDetail(std::move(error));
            return NEXVR_ERROR_RUNTIME_FAILURE;
        }

        SetDetail({});
        return NEXVR_SUCCESS;
    });
}

NEXVR_API NexVR_Result NEXVR_CALL
NexVR_GetControllerState(NexVR_Session session, NexVR_Hand hand, NexVR_ControllerState* outState) {
    return SafeApiCall([=]() -> NexVR_Result {
        auto* impl = reinterpret_cast<SessionImpl*>(session);
        if (impl == nullptr) {
            SetDetail("session was null");
            return NEXVR_ERROR_INVALID_SESSION;
        }

        std::string detail;
        const NexVR_Result verdict = nexvr::sdk::ValidateControllerState(hand, outState, detail);
        if (NEXVR_FAILED(verdict)) {
            SetDetail(std::move(detail));
            return verdict;
        }

        std::string error;
        if (!impl->core.GetControllerState(hand, *outState, error)) {
            SetDetail(std::move(error));
            return NEXVR_ERROR_RUNTIME_FAILURE;
        }

        SetDetail({});
        return NEXVR_SUCCESS;
    });
}

NEXVR_API NexVR_Result NEXVR_CALL
NexVR_TriggerHaptic(NexVR_Session session, NexVR_Hand hand, const NexVR_HapticFeedback* haptic) {
    return SafeApiCall([=]() -> NexVR_Result {
        auto* impl = reinterpret_cast<SessionImpl*>(session);
        if (impl == nullptr) {
            SetDetail("session was null");
            return NEXVR_ERROR_INVALID_SESSION;
        }

        if (haptic == nullptr) {
            return NexVR_StopHaptic(session, hand);
        }

        std::string detail;
        const NexVR_Result verdict = nexvr::sdk::ValidateHapticFeedback(hand, haptic, detail);
        if (NEXVR_FAILED(verdict)) {
            SetDetail(std::move(detail));
            return verdict;
        }

        std::string error;
        if (!impl->core.TriggerHaptic(hand, *haptic, error)) {
            SetDetail(std::move(error));
            return NEXVR_ERROR_RUNTIME_FAILURE;
        }

        SetDetail({});
        return NEXVR_SUCCESS;
    });
}

NEXVR_API NexVR_Result NEXVR_CALL
NexVR_StopHaptic(NexVR_Session session, NexVR_Hand hand) {
    return SafeApiCall([=]() -> NexVR_Result {
        auto* impl = reinterpret_cast<SessionImpl*>(session);
        if (impl == nullptr) {
            SetDetail("session was null");
            return NEXVR_ERROR_INVALID_SESSION;
        }

        if (hand != NEXVR_HAND_LEFT && hand != NEXVR_HAND_RIGHT) {
            SetDetail("hand must be NEXVR_HAND_LEFT (0) or NEXVR_HAND_RIGHT (1)");
            return NEXVR_ERROR_INVALID_ARGUMENT;
        }

        std::string error;
        if (!impl->core.StopHaptic(hand, error)) {
            SetDetail(std::move(error));
            return NEXVR_ERROR_RUNTIME_FAILURE;
        }

        SetDetail({});
        return NEXVR_SUCCESS;
    });
}

NEXVR_API void NEXVR_CALL NexVR_Shutdown(NexVR_Session session) {
    SafeApiCallVoid([=]() {
        auto* impl = reinterpret_cast<SessionImpl*>(session);
        if (impl == nullptr) {
            return;
        }
        delete impl;
    });
}

NEXVR_API const char* NEXVR_CALL NexVR_GetLastErrorDetail(void) {
    return g_lastError.c_str();
}

// ===========================================================================
// STEREIX ENTERPRISE B2B C ABI EXPORTS
// ===========================================================================

STEREIX_API Stereix_Result STEREIX_CALL
Stereix_GetGraphicsRequirements(Stereix_GraphicsRequirements* requirements) {
    return NexVR_GetGraphicsRequirements(requirements);
}

STEREIX_API Stereix_Result STEREIX_CALL
Stereix_GetGraphicsRequirementsDX12(Stereix_GraphicsRequirementsDX12* requirements) {
    return NexVR_GetGraphicsRequirementsDX12(requirements);
}

STEREIX_API Stereix_Result STEREIX_CALL
Stereix_InitializeDX11(const Stereix_InitializeDX11Info* info, Stereix_Session* outSession) {
    return NexVR_InitializeDX11(info, outSession);
}

STEREIX_API Stereix_Result STEREIX_CALL
Stereix_InitializeDX12(const Stereix_InitializeDX12Info* info, Stereix_Session* outSession) {
    return NexVR_InitializeDX12(info, outSession);
}

STEREIX_API Stereix_Result STEREIX_CALL
Stereix_WaitFrame(Stereix_Session session, Stereix_FrameState* frameState) {
    return NexVR_WaitFrame(session, frameState);
}

STEREIX_API Stereix_Result STEREIX_CALL
Stereix_SubmitFrameDX11(Stereix_Session session, uint64_t frameToken, ID3D11Texture2D* renderTarget) {
    return NexVR_SubmitFrameDX11(session, frameToken, renderTarget);
}

STEREIX_API Stereix_Result STEREIX_CALL
Stereix_SubmitFrameWithDepthDX11(Stereix_Session session, uint64_t frameToken,
                                 ID3D11Texture2D* renderTarget, const Stereix_DepthInfoDX11* depthInfo) {
    return NexVR_SubmitFrameWithDepthDX11(session, frameToken, renderTarget, depthInfo);
}

STEREIX_API uint32_t STEREIX_CALL
Stereix_SupportsDepthSubmission(Stereix_Session session) {
    return NexVR_SupportsDepthSubmission(session);
}

STEREIX_API Stereix_Result STEREIX_CALL
Stereix_SubmitFrameDX12(Stereix_Session session, uint64_t frameToken,
                        ID3D12Resource* renderTarget, D3D12_RESOURCE_STATES targetState) {
    return NexVR_SubmitFrameDX12(session, frameToken, renderTarget, targetState);
}

STEREIX_API Stereix_Result STEREIX_CALL
Stereix_SubmitFrameWithDepthDX12(Stereix_Session session, uint64_t frameToken,
                                 ID3D12Resource* renderTarget, D3D12_RESOURCE_STATES targetState,
                                 const Stereix_DepthInfoDX12* depthInfo) {
    return NexVR_SubmitFrameWithDepthDX12(session, frameToken, renderTarget, targetState, depthInfo);
}

STEREIX_API Stereix_Result STEREIX_CALL
Stereix_SyncInput(Stereix_Session session) {
    return NexVR_SyncInput(session);
}

STEREIX_API Stereix_Result STEREIX_CALL
Stereix_GetControllerState(Stereix_Session session, Stereix_Hand hand, Stereix_ControllerState* outState) {
    return NexVR_GetControllerState(session, hand, outState);
}

STEREIX_API Stereix_Result STEREIX_CALL
Stereix_TriggerHaptic(Stereix_Session session, Stereix_Hand hand, const Stereix_HapticFeedback* haptic) {
    return NexVR_TriggerHaptic(session, hand, haptic);
}

STEREIX_API Stereix_Result STEREIX_CALL
Stereix_StopHaptic(Stereix_Session session, Stereix_Hand hand) {
    return NexVR_StopHaptic(session, hand);
}

STEREIX_API void STEREIX_CALL
Stereix_Shutdown(Stereix_Session session) {
    NexVR_Shutdown(session);
}

STEREIX_API const char* STEREIX_CALL
Stereix_GetLastErrorDetail(void) {
    return NexVR_GetLastErrorDetail();
}

}  // extern "C"
