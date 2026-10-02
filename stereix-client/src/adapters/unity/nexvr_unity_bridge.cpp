// The Unity native plugin bridge.
//
// ===========================================================================
// WHAT THIS SOLVES
// ===========================================================================
//
// NexVR_SubmitFrameDX11 must run on the thread that owns the D3D11 immediate
// context. In Unity that is the Render Thread, and C# script code never runs
// there — a MonoBehaviour runs on the Game Thread, which is a different thread
// entirely.
//
// Unity's only sanctioned way across that boundary is
// CommandBuffer.IssuePluginEvent, which schedules a native callback onto the
// Render Thread in command order. So this file exists to be the far side of
// that callback: C# stages a frame from the Game Thread, issues a plugin event,
// and the callback below submits it from the Render Thread.
//
// ===========================================================================
// WHY THERE ARE NO UNITY HEADERS HERE
// ===========================================================================
//
// The usual approach vendors IUnityInterface.h and IUnityGraphicsD3D11.h to
// obtain the ID3D11Device. This bridge does not, for two reasons:
//
//   - Those headers ship with a Unity installation and carry Unity's licence
//     terms. Copying them into this repository creates a redistribution
//     question for a file we barely need.
//   - IUnityGraphicsD3D11 is a COM-style interface whose vtable we would have
//     to match exactly, across Unity versions, forever.
//
// Neither is necessary. The render target Unity hands us already knows which
// device it was created on — ID3D11Texture2D::GetDevice answers it. So the only
// Unity ABI this file depends on is the shape of the render-event callback,
// which is a plain `void __stdcall (int)` and has been stable for a decade.
//
// The cost is that this plugin cannot receive Unity's device-lifecycle events.
// That is handled explicitly instead: C# calls Shutdown before the device goes
// away, and the SDK holds a reference so a device cannot vanish mid-frame.

#include "nexvr_sdk.h"

#include <d3d11.h>
#include <d3d12.h>

#include <array>
#include <atomic>
#include <cstdint>

/**
 * Unity's render-event callback signature.
 *
 * Declared here rather than included from IUnityInterface.h, which is the only
 * thing this bridge would otherwise have needed that header for. It is a plain
 * void(int) and has been stable across every Unity version that supports
 * CommandBuffer.IssuePluginEvent.
 */
using UnityRenderingEvent = void(__stdcall*)(int eventId);

namespace {

/**
 * One frame staged by the Game Thread, waiting for the Render Thread.
 *
 * The token and the texture must travel together. Unity's IssuePluginEvent
 * carries only an int, so the int is an index into this ring rather than
 * anything meaningful on its own.
 */
enum class SlotState : int {
    Free = 0,     /**< Nobody owns it. */
    Filling = 1,  /**< The Game Thread has claimed it and is writing. */
    Ready = 2,    /**< Fully written; the Render Thread may consume it. */
};

enum class BridgeApi : int {
    D3D11 = 0,
    D3D12 = 1,
};

struct StagedFrame {
    BridgeApi api{BridgeApi::D3D11};
    uint64_t token{0};
    void* colorResource{nullptr};
    void* depthResource{nullptr};
    uint32_t colorState{0};
    uint32_t depthState{0};
    float nearZ{0.0f};
    float farZ{0.0f};
    NexVR_DepthRange depthRange{NEXVR_DEPTH_RANGE_ZERO_TO_ONE};
    bool hasDepth{false};

    /**
     * Three states, not a bool.
     *
     * A bool cannot distinguish "claimed but not yet written" from "free", so
     * the Render Thread could read a slot whose token and texture were still
     * being filled in. Filling is what makes the claim and the write two
     * separate, observable steps.
     */
    std::atomic<SlotState> state{SlotState::Free};
};

/**
 * A small fixed ring, NOT a queue that grows.
 *
 * Unity's render thread runs one or two frames behind the game thread, so a
 * handful of slots is always enough. A growing queue would hide the case this
 * ring makes obvious and survivable: if the Render Thread stalls, the Game
 * Thread must be told to stop producing rather than quietly accumulating frames
 * whose textures Unity may already have recycled.
 *
 * Deliberately not std::deque with a mutex on the Game Thread's path either.
 * Staging happens every frame in Update(); taking a lock there to talk to a
 * thread that is not contending for it is cost with no benefit.
 */
constexpr int kRingSize = 8;
std::array<StagedFrame, kRingSize> g_ring;

/** Set once by NexVR_Unity_Attach; read by the Render Thread every event. */
std::atomic<NexVR_Session> g_session{nullptr};

/** Counters the C# layer can poll. Diagnostics, never control flow. */
std::atomic<uint32_t> g_submitted{0};
std::atomic<uint32_t> g_dropped{0};
std::atomic<uint32_t> g_failed{0};
std::atomic<int32_t> g_lastResult{NEXVR_SUCCESS};

/**
 * The Render Thread callback Unity invokes for IssuePluginEvent.
 *
 * __stdcall on x64 Windows is ignored by the ABI, but it is written out because
 * it is what Unity's UnityRenderingEvent typedef declares and a mismatch on any
 * future target would be a silent stack corruption.
 *
 * THIS RUNS ON UNITY'S RENDER THREAD. Everything it touches must be safe there:
 * no allocation, no Unity API, no C# callback, no logging that takes a lock.
 */
void __stdcall OnRenderEvent(int eventId) {
    if (eventId < 0 || eventId >= kRingSize) {
        return;
    }

    StagedFrame& frame = g_ring[static_cast<size_t>(eventId)];

    // Acquire pairs with the release in NexVR_Unity_StageFrame, so the token
    // and texture writes are visible before this thread reads them.
    if (frame.state.load(std::memory_order_acquire) != SlotState::Ready) {
        // Unity issued an event for a slot the Game Thread never filled. Not
        // fatal and not worth a stall — but counted, because silently doing
        // nothing here is how a whole session renders black with no error.
        g_failed.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    NexVR_Session session = g_session.load(std::memory_order_acquire);
    if (session == nullptr) {
        frame.state.store(SlotState::Free, std::memory_order_release);
        return;
    }

    NexVR_Result result = NEXVR_SUCCESS;
    if (frame.api == BridgeApi::D3D12) {
        auto* colorRes = static_cast<ID3D12Resource*>(frame.colorResource);
        auto colorState = static_cast<D3D12_RESOURCE_STATES>(frame.colorState);

        if (frame.hasDepth && frame.depthResource != nullptr) {
            NexVR_DepthInfoDX12 depthInfo{};
            depthInfo.structSize = sizeof(NexVR_DepthInfoDX12);
            depthInfo.depthTexture = static_cast<ID3D12Resource*>(frame.depthResource);
            depthInfo.nearZ = frame.nearZ;
            depthInfo.farZ = frame.farZ;
            depthInfo.range = frame.depthRange;
            depthInfo.depthState = static_cast<D3D12_RESOURCE_STATES>(frame.depthState);
            result = NexVR_SubmitFrameWithDepthDX12(session, frame.token, colorRes, colorState, &depthInfo);
        } else {
            result = NexVR_SubmitFrameDX12(session, frame.token, colorRes, colorState);
        }
    } else {
        auto* colorTex = static_cast<ID3D11Texture2D*>(frame.colorResource);

        if (frame.hasDepth && frame.depthResource != nullptr) {
            NexVR_DepthInfoDX11 depthInfo{};
            depthInfo.structSize = sizeof(NexVR_DepthInfoDX11);
            depthInfo.depthTexture = static_cast<ID3D11Texture2D*>(frame.depthResource);
            depthInfo.nearZ = frame.nearZ;
            depthInfo.farZ = frame.farZ;
            depthInfo.range = frame.depthRange;
            result = NexVR_SubmitFrameWithDepthDX11(session, frame.token, colorTex, &depthInfo);
        } else {
            result = NexVR_SubmitFrameDX11(session, frame.token, colorTex);
        }
    }

    g_lastResult.store(static_cast<int32_t>(result), std::memory_order_relaxed);

    if (result == NEXVR_ERROR_SWAPCHAIN_TIMEOUT) {
        // Survivable by contract. The compositor did not free an image in time
        // and the SDK dropped the frame rather than blocking this thread —
        // which is the entire point of the Phase 0.9 lock ordering, since
        // blocking here would freeze Unity's renderer.
        g_dropped.fetch_add(1, std::memory_order_relaxed);
    } else if (NEXVR_FAILED(result)) {
        g_failed.fetch_add(1, std::memory_order_relaxed);
    } else {
        g_submitted.fetch_add(1, std::memory_order_relaxed);
    }

    // Released last, so the Game Thread cannot reuse this slot until the
    // Render Thread is genuinely finished with the texture pointer in it.
    frame.state.store(SlotState::Free, std::memory_order_release);
}

}  // namespace

extern "C" {

/**
 * The function pointer Unity needs for CommandBuffer.IssuePluginEvent.
 *
 * Named GetRenderEventFunc by convention — every Unity native plugin example
 * uses it, and matching that convention is worth more than a better name.
 */
__declspec(dllexport) UnityRenderingEvent __stdcall NexVR_Unity_GetRenderEventFunc() {
    return &OnRenderEvent;
}

/**
 * Hand the bridge a session created by the C# layer.
 *
 * The bridge deliberately does NOT create the session itself. Initialization
 * needs the adapter LUID and render size BEFORE Unity's device exists (see
 * docs/B2B_UNITY_INTEGRATION_PLAN.md), which is a decision only the C# layer —
 * and in the multi-GPU case, only the person launching the editor — can make.
 * A bridge that quietly initialised on first use would bury that.
 */
__declspec(dllexport) void __stdcall NexVR_Unity_Attach(NexVR_Session session) {
    g_session.store(session, std::memory_order_release);
    g_submitted.store(0, std::memory_order_relaxed);
    g_dropped.store(0, std::memory_order_relaxed);
    g_failed.store(0, std::memory_order_relaxed);
    g_lastResult.store(NEXVR_SUCCESS, std::memory_order_relaxed);
}

/**
 * Detach before the session is destroyed. Call from the Game Thread.
 *
 * Does not free the session — the C# layer owns it and calls NexVR_Shutdown.
 * This only guarantees the Render Thread stops using it, which must happen
 * first: a render event already queued behind this call would otherwise submit
 * against a destroyed session.
 */
__declspec(dllexport) void __stdcall NexVR_Unity_Detach() {
    g_session.store(nullptr, std::memory_order_release);
    for (StagedFrame& frame : g_ring) {
        frame.state.store(SlotState::Free, std::memory_order_release);
    }
}

/**
 * Stage a frame with an optional depth buffer from the GAME THREAD (D3D11).
 *
 * @param token         From NexVR_WaitFrame, on this same thread.
 * @param texture       Unity's color RenderTexture.GetNativeTexturePtr().
 * @param depthTexture  Unity's depth buffer native pointer, or nullptr if color-only.
 * @param nearZ         Near clip plane in meters (positive).
 * @param farZ          Far clip plane in meters (positive, or INFINITY).
 * @param depthRange    0 = ZeroToOne, 1 = Reversed (Unity D3D11 default).
 * @return              The event id for CommandBuffer.IssuePluginEvent, or -1 when full.
 */
__declspec(dllexport) int __stdcall NexVR_Unity_StageFrameWithDepth(
    uint64_t token, void* texture, void* depthTexture, float nearZ, float farZ, int depthRange) {
    if (texture == nullptr) {
        return -1;
    }

    for (int slot = 0; slot < kRingSize; ++slot) {
        StagedFrame& frame = g_ring[static_cast<size_t>(slot)];

        // Free -> Filling claims the slot exclusively. An earlier version of
        // this did compare_exchange(false, false), which sets a false flag to
        // false and therefore claims nothing — two callers would both
        // "succeed" and write over each other's token.
        SlotState expected = SlotState::Free;
        if (!frame.state.compare_exchange_strong(expected, SlotState::Filling,
                                                 std::memory_order_acquire,
                                                 std::memory_order_relaxed)) {
            continue;
        }

        frame.api = BridgeApi::D3D11;
        frame.token = token;
        frame.colorResource = texture;
        frame.depthResource = depthTexture;
        frame.colorState = 0;
        frame.depthState = 0;
        frame.nearZ = nearZ;
        frame.farZ = farZ;
        frame.depthRange = (depthRange == 1) ? NEXVR_DEPTH_RANGE_REVERSED : NEXVR_DEPTH_RANGE_ZERO_TO_ONE;
        frame.hasDepth = (depthTexture != nullptr);

        // Released AFTER all writes, so the Render Thread's acquire cannot
        // observe Ready with a stale token, texture, or depth behind it.
        frame.state.store(SlotState::Ready, std::memory_order_release);
        return slot;
    }
    return -1;
}

/**
 * Stage a frame from the GAME THREAD, returning the event id to issue.
 * Backwards-compatible entry point that delegates with no depth buffer.
 */
__declspec(dllexport) int __stdcall NexVR_Unity_StageFrame(uint64_t token, void* texture) {
    return NexVR_Unity_StageFrameWithDepth(token, texture, nullptr, 0.0f, 0.0f, 0);
}

/**
 * Stage a frame with an optional depth buffer from the GAME THREAD (D3D12).
 */
__declspec(dllexport) int __stdcall NexVR_Unity_StageFrameWithDepthDX12(
    uint64_t token, void* colorResource, uint32_t colorState,
    void* depthResource, uint32_t depthState,
    float nearZ, float farZ, int depthRange) {
    if (colorResource == nullptr) {
        return -1;
    }

    for (int slot = 0; slot < kRingSize; ++slot) {
        StagedFrame& frame = g_ring[static_cast<size_t>(slot)];

        SlotState expected = SlotState::Free;
        if (!frame.state.compare_exchange_strong(expected, SlotState::Filling,
                                                 std::memory_order_acquire,
                                                 std::memory_order_relaxed)) {
            continue;
        }

        frame.api = BridgeApi::D3D12;
        frame.token = token;
        frame.colorResource = colorResource;
        frame.depthResource = depthResource;
        frame.colorState = colorState;
        frame.depthState = depthState;
        frame.nearZ = nearZ;
        frame.farZ = farZ;
        frame.depthRange = (depthRange == 1) ? NEXVR_DEPTH_RANGE_REVERSED : NEXVR_DEPTH_RANGE_ZERO_TO_ONE;
        frame.hasDepth = (depthResource != nullptr);

        frame.state.store(SlotState::Ready, std::memory_order_release);
        return slot;
    }
    return -1;
}

/**
 * Stage a frame from the GAME THREAD (D3D12) with no depth.
 */
__declspec(dllexport) int __stdcall NexVR_Unity_StageFrameDX12(
    uint64_t token, void* colorResource, uint32_t colorState) {
    return NexVR_Unity_StageFrameWithDepthDX12(token, colorResource, colorState, nullptr, 0, 0.0f, 0.0f, 0);
}

/**
 * Query whether the current attached session supports depth submission.
 */
__declspec(dllexport) int __stdcall NexVR_Unity_SupportsDepthSubmission() {
    NexVR_Session session = g_session.load(std::memory_order_acquire);
    if (session == nullptr) {
        return 0;
    }
    return NexVR_SupportsDepthSubmission(session) ? 1 : 0;
}

/**
 * The device Unity created, recovered from a texture Unity created on it.
 *
 * This is why the bridge needs no Unity headers. A studio's C# layer passes any
 * RenderTexture's native pointer and gets back the ID3D11Device to hand to
 * NexVR_InitializeDX11 — no IUnityGraphicsD3D11, no vtable to match, no Unity
 * version coupling.
 *
 * The immediate context comes with it, because a device only has one.
 */
__declspec(dllexport) int __stdcall NexVR_Unity_GetDeviceFromTexture(void* texture, void** outDevice,
                                                                    void** outContext) {
    if (texture == nullptr || outDevice == nullptr || outContext == nullptr) {
        return 0;
    }
    *outDevice = nullptr;
    *outContext = nullptr;

    auto* typed = static_cast<ID3D11Texture2D*>(texture);
    ID3D11Device* device = nullptr;
    typed->GetDevice(&device);
    if (device == nullptr) {
        return 0;
    }

    ID3D11DeviceContext* context = nullptr;
    device->GetImmediateContext(&context);
    if (context == nullptr) {
        device->Release();
        return 0;
    }

    // Both returned with a reference the caller must release through
    // NexVR_Unity_ReleaseDeviceHandles. GetDevice and GetImmediateContext both
    // AddRef, and leaking a device reference keeps Unity's whole graphics stack
    // alive past shutdown.
    *outDevice = device;
    *outContext = context;
    return 1;
}

/** Releases what NexVR_Unity_GetDeviceFromTexture handed out. */
__declspec(dllexport) void __stdcall NexVR_Unity_ReleaseDeviceHandles(void* device, void* context) {
    if (context != nullptr) {
        static_cast<ID3D11DeviceContext*>(context)->Release();
    }
    if (device != nullptr) {
        static_cast<ID3D11Device*>(device)->Release();
    }
}

/**
 * The D3D12 device Unity created, recovered from an ID3D12Resource.
 */
__declspec(dllexport) int __stdcall NexVR_Unity_GetDeviceFromResourceDX12(
    void* resource, void** outDevice) {
    if (resource == nullptr || outDevice == nullptr) {
        return 0;
    }
    *outDevice = nullptr;

    auto* typed = static_cast<ID3D12Resource*>(resource);
    ID3D12Device* device = nullptr;
    HRESULT hr = typed->GetDevice(IID_PPV_ARGS(&device));
    if (FAILED(hr) || device == nullptr) {
        return 0;
    }

    *outDevice = device;
    return 1;
}

/** Releases what NexVR_Unity_GetDeviceFromResourceDX12 handed out. */
__declspec(dllexport) void __stdcall NexVR_Unity_ReleaseDeviceHandleDX12(void* device) {
    if (device != nullptr) {
        static_cast<ID3D12Device*>(device)->Release();
    }
}

/** Diagnostics for the C# layer. Never used to make decisions in native code. */
__declspec(dllexport) void __stdcall NexVR_Unity_GetStats(uint32_t* submitted, uint32_t* dropped,
                                                          uint32_t* failed, int32_t* lastResult) {
    if (submitted != nullptr) {
        *submitted = g_submitted.load(std::memory_order_relaxed);
    }
    if (dropped != nullptr) {
        *dropped = g_dropped.load(std::memory_order_relaxed);
    }
    if (failed != nullptr) {
        *failed = g_failed.load(std::memory_order_relaxed);
    }
    if (lastResult != nullptr) {
        *lastResult = g_lastResult.load(std::memory_order_relaxed);
    }
}

/** Synchronize OpenXR input actions for the attached session. */
__declspec(dllexport) int __stdcall NexVR_Unity_SyncInput() {
    NexVR_Session session = g_session.load(std::memory_order_acquire);
    if (session == nullptr) {
        return NEXVR_ERROR_INVALID_SESSION;
    }
    return NexVR_SyncInput(session);
}

/** Read full 6DOF controller tracking and button/analog input state. */
__declspec(dllexport) int __stdcall NexVR_Unity_GetControllerState(
    NexVR_Hand hand, NexVR_ControllerState* outState) {
    if (outState == nullptr) {
        return NEXVR_ERROR_INVALID_ARGUMENT;
    }
    NexVR_Session session = g_session.load(std::memory_order_acquire);
    if (session == nullptr) {
        return NEXVR_ERROR_INVALID_SESSION;
    }
    return NexVR_GetControllerState(session, hand, outState);
}

/** Trigger vibration haptic feedback on the specified hand controller. */
__declspec(dllexport) int __stdcall NexVR_Unity_TriggerHaptic(
    NexVR_Hand hand, const NexVR_HapticFeedback* haptic) {
    if (haptic == nullptr) {
        return NEXVR_ERROR_INVALID_ARGUMENT;
    }
    NexVR_Session session = g_session.load(std::memory_order_acquire);
    if (session == nullptr) {
        return NEXVR_ERROR_INVALID_SESSION;
    }
    return NexVR_TriggerHaptic(session, hand, haptic);
}

/** Stop any currently running haptic vibration on the specified hand. */
__declspec(dllexport) int __stdcall NexVR_Unity_StopHaptic(NexVR_Hand hand) {
    NexVR_Session session = g_session.load(std::memory_order_acquire);
    if (session == nullptr) {
        return NEXVR_ERROR_INVALID_SESSION;
    }
    return NexVR_StopHaptic(session, hand);
}

// ===========================================================================
// Stereix Enterprise Unity Bridge C ABI Forwarders
// ===========================================================================

__declspec(dllexport) UnityRenderingEvent __stdcall Stereix_Unity_GetRenderEventFunc() {
    return NexVR_Unity_GetRenderEventFunc();
}

__declspec(dllexport) void __stdcall Stereix_Unity_Attach(NexVR_Session session) {
    NexVR_Unity_Attach(session);
}

__declspec(dllexport) void __stdcall Stereix_Unity_Detach() {
    NexVR_Unity_Detach();
}

__declspec(dllexport) int __stdcall Stereix_Unity_StageFrameWithDepth(
    uint64_t token, void* texture, void* depthTexture, float nearZ, float farZ, int depthRange) {
    return NexVR_Unity_StageFrameWithDepth(token, texture, depthTexture, nearZ, farZ, depthRange);
}

__declspec(dllexport) int __stdcall Stereix_Unity_StageFrame(uint64_t token, void* texture) {
    return NexVR_Unity_StageFrame(token, texture);
}

__declspec(dllexport) int __stdcall Stereix_Unity_StageFrameWithDepthDX12(
    uint64_t token, void* colorResource, uint32_t colorState,
    void* depthResource, uint32_t depthState,
    float nearZ, float farZ, int depthRange) {
    return NexVR_Unity_StageFrameWithDepthDX12(token, colorResource, colorState, depthResource, depthState, nearZ, farZ, depthRange);
}

__declspec(dllexport) int __stdcall Stereix_Unity_StageFrameDX12(
    uint64_t token, void* colorResource, uint32_t colorState) {
    return NexVR_Unity_StageFrameDX12(token, colorResource, colorState);
}

__declspec(dllexport) int __stdcall Stereix_Unity_SupportsDepthSubmission() {
    return NexVR_Unity_SupportsDepthSubmission();
}

__declspec(dllexport) int __stdcall Stereix_Unity_GetDeviceFromTexture(void* texture, void** outDevice, void** outContext) {
    return NexVR_Unity_GetDeviceFromTexture(texture, outDevice, outContext);
}

__declspec(dllexport) void __stdcall Stereix_Unity_ReleaseDeviceHandles(void* device, void* context) {
    NexVR_Unity_ReleaseDeviceHandles(device, context);
}

__declspec(dllexport) int __stdcall Stereix_Unity_GetDeviceFromResourceDX12(void* resource, void** outDevice) {
    return NexVR_Unity_GetDeviceFromResourceDX12(resource, outDevice);
}

__declspec(dllexport) void __stdcall Stereix_Unity_ReleaseDeviceHandleDX12(void* device) {
    NexVR_Unity_ReleaseDeviceHandleDX12(device);
}

__declspec(dllexport) void __stdcall Stereix_Unity_GetStats(uint32_t* submitted, uint32_t* dropped,
                                                           uint32_t* failed, int32_t* lastResult) {
    NexVR_Unity_GetStats(submitted, dropped, failed, lastResult);
}

__declspec(dllexport) int __stdcall Stereix_Unity_SyncInput() {
    return NexVR_Unity_SyncInput();
}

__declspec(dllexport) int __stdcall Stereix_Unity_GetControllerState(
    NexVR_Hand hand, NexVR_ControllerState* outState) {
    return NexVR_Unity_GetControllerState(hand, outState);
}

__declspec(dllexport) int __stdcall Stereix_Unity_TriggerHaptic(
    NexVR_Hand hand, const NexVR_HapticFeedback* haptic) {
    return NexVR_Unity_TriggerHaptic(hand, haptic);
}

__declspec(dllexport) int __stdcall Stereix_Unity_StopHaptic(NexVR_Hand hand) {
    return NexVR_Unity_StopHaptic(hand);
}

}  // extern "C"
