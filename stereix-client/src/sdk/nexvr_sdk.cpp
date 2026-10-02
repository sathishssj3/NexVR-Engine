// The C ABI, and deliberately almost nothing else.
//
// Every function here does three things: check its arguments, call into
// SdkSession, and translate the answer into a NexVR_Result. Logic that lives
// in this file is logic that cannot be unit-tested without loading a DLL and
// standing up a session, so there is as little of it as possible.
//
// The one thing this layer genuinely owns is the boundary discipline: no
// exception may cross it, no C++ type may appear in a signature, and no
// pointer handed to us is trusted without being checked.

#include "nexvr_sdk.h"

#include "sdk_session.h"
#include "sdk_validation.h"

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

}  // namespace

extern "C" {

NEXVR_API NexVR_Result NEXVR_CALL
NexVR_GetGraphicsRequirements(NexVR_GraphicsRequirements* requirements) {
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
        // Distinguished, because the two demand completely different things of
        // the user: install a runtime, versus plug in a headset.
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

    // Precomputed so an integrator cannot get the arithmetic wrong. Phase 0.8
    // lost 30 frames to exactly this multiplication.
    requirements->requiredTextureWidth = found.eyeWidth * found.viewCount;
    requirements->requiredTextureHeight = found.eyeHeight;

    // UNKNOWN, honestly: the swapchain format cannot be enumerated without a
    // session, which cannot exist without the device this call precedes. The
    // real format is chosen at InitializeDX11 to be compatible with whatever
    // the game brings, so there is nothing to promise here.
    requirements->swapchainFormat = DXGI_FORMAT_UNKNOWN;

    g_constraints.adapterLuid = found.adapterLuid;
    g_constraints.requiredWidth = requirements->requiredTextureWidth;
    g_constraints.requiredHeight = requirements->requiredTextureHeight;
    g_constraints.swapchainFormat = DXGI_FORMAT_UNKNOWN;
    g_constraints.queried = true;

    SetDetail({});
    return NEXVR_SUCCESS;
}

NEXVR_API NexVR_Result NEXVR_CALL
NexVR_GetGraphicsRequirementsDX12(NexVR_GraphicsRequirements* requirements) {
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
}

NEXVR_API NexVR_Result NEXVR_CALL
NexVR_InitializeDX11(const NexVR_InitializeDX11Info* info, NexVR_Session* outSession) {
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

    // Re-queried rather than reusing the probe's instance, because the session
    // needs its OWN XrInstance to hold for its lifetime. The answer is
    // discarded — g_constraints already holds what the game was told, and
    // trusting a second reply over the first would let the runtime change its
    // mind after the game had already built its renderer around the first.
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

    // =====================================================================
    // EVERY CONSTRAINT, CHECKED BEFORE ANY OPENXR OBJECT EXISTS
    // =====================================================================
    // A rejected integration attempt must leave the runtime exactly as it was.
    // Creating a session and then discovering the target is the wrong size
    // would leave a studio's next attempt fighting a session they cannot see.
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
}

NEXVR_API NexVR_Result NEXVR_CALL
NexVR_InitializeDX12(const NexVR_InitializeDX12Info* info, NexVR_Session* outSession) {
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
}

NEXVR_API NexVR_Result NEXVR_CALL
NexVR_WaitFrame(NexVR_Session session, NexVR_FrameState* outFrame) {
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
        // Not an error. The runtime has not moved the session to a
        // frame-submitting state yet, and an engine polling during startup
        // should keep ticking rather than tear down.
        //
        // Routed through PublishFrameState rather than zeroing two fields by
        // hand, so that this path clears viewCount like every other one. It did
        // not, and a caller reusing one NexVR_FrameState across a session
        // restart would have read the last good frame's viewCount beside poses
        // from before the restart.
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

    // The poses cross the boundary HERE, and only here. This copy is what was
    // missing: views[] was declared in the public struct, viewCount was set to
    // 2, and the array was never written, so a game read its own uninitialised
    // stack as a head pose while every call returned NEXVR_SUCCESS.
    return nexvr::sdk::PublishFrameState(handoff.shouldRender, handoff.posesValid,
                                         handoff.viewCount, handoff.views, *outFrame);
}

NEXVR_API NexVR_Result NEXVR_CALL
NexVR_SubmitFrameDX11(NexVR_Session session, uint64_t token, ID3D11Texture2D* colorTexture) {
    // The pre-depth entry point, unchanged in signature and behaviour. Every
    // binary already linked against it keeps working; it simply asks for no
    // depth. Keeping it as a forwarder rather than a copy means the two can
    // never drift.
    return NexVR_SubmitFrameWithDepthDX11(session, token, colorTexture, nullptr);
}

NEXVR_API uint32_t NEXVR_CALL NexVR_SupportsDepthSubmission(NexVR_Session session) {
    auto* impl = reinterpret_cast<SessionImpl*>(session);
    return impl != nullptr && impl->core.DepthUsable() ? 1u : 0u;
}

NEXVR_API NexVR_Result NEXVR_CALL
NexVR_SubmitFrameWithDepthDX11(NexVR_Session session, uint64_t token,
                               ID3D11Texture2D* colorTexture, const NexVR_DepthInfoDX11* depth) {
    auto* impl = reinterpret_cast<SessionImpl*>(session);
    if (impl == nullptr) {
        SetDetail("session was null");
        return NEXVR_ERROR_INVALID_SESSION;
    }
    // Which checks apply depends on the frame, not on the arguments.
    //
    // A frame the runtime declined to display still has to be submitted so that
    // OpenXR ends the frame it began, and the game has no finished render
    // target for a frame it was told not to render. So a null texture is legal
    // there and a caller bug anywhere else — and the only way to tell those
    // apart is to ask what the token was issued for.
    //
    // A token that is not outstanding leaves this false, which makes the checks
    // strict; SubmitFrameThreaded then rejects the token by name a few lines
    // below, which is the more useful of the two complaints.
    bool shouldRender = false;
    (void)impl->core.TokenShouldRender(token, shouldRender);

    std::string detail;
    const NexVR_Result admissible = nexvr::sdk::ValidateSubmit(
        shouldRender, DescribeTexture(colorTexture), impl->constraints, detail);
    if (NEXVR_FAILED(admissible)) {
        SetDetail(std::move(detail));
        return admissible;
    }

    // --- Depth, entirely optional ---------------------------------------
    //
    // Skipped when the caller passed none, when the frame is not being
    // rendered, or when the runtime cannot take depth. That last case is a
    // silent skip on purpose: a game should not have to branch on runtime
    // capabilities to render normally, and failing a frame over an unavailable
    // OPTIONAL extension would turn a quality improvement into a hard
    // compatibility requirement.
    nexvr::sdk::SdkSession::DepthSubmission submission{};
    const nexvr::sdk::SdkSession::DepthSubmission* depthPtr = nullptr;

    if (depth != nullptr && depth->depthTexture != nullptr && shouldRender &&
        impl->core.DepthUsable()) {
        // Validated even though it may then be dropped. A caller who supplied
        // a broken depth buffer deserves to be told, rather than to have it
        // quietly ignored and wonder why reprojection never improved.
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
        // A token the core does not recognise is a pairing violation, which is
        // a caller bug worth naming precisely rather than folding into a
        // generic runtime failure.
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
}

NEXVR_API NexVR_Result NEXVR_CALL NexVR_SyncInput(NexVR_Session session) {
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
}

NEXVR_API NexVR_Result NEXVR_CALL
NexVR_GetControllerState(NexVR_Session session, NexVR_Hand hand, NexVR_ControllerState* outState) {
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
}

NEXVR_API NexVR_Result NEXVR_CALL
NexVR_TriggerHaptic(NexVR_Session session, NexVR_Hand hand, const NexVR_HapticFeedback* haptic) {
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
}

NEXVR_API NexVR_Result NEXVR_CALL
NexVR_StopHaptic(NexVR_Session session, NexVR_Hand hand) {
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
}

NEXVR_API void NEXVR_CALL NexVR_Shutdown(NexVR_Session session) {
    auto* impl = reinterpret_cast<SessionImpl*>(session);
    if (impl == nullptr) {
        return;
    }
    // SdkSession::Shutdown destroys the OpenXR objects first and drops its
    // reference to the game's device last. Deleting the wrapper runs it.
    delete impl;
}

NEXVR_API const char* NEXVR_CALL NexVR_GetLastErrorDetail(void) {
    // Never null. A caller printing this should not have to null-check a
    // diagnostic function.
    return g_lastError.c_str();
}

}  // extern "C"
