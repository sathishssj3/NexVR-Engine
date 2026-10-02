// The Unreal Engine native RHI plugin bridge.
//
// ===========================================================================
// WHAT THIS SOLVES
// ===========================================================================
//
// NexVR frame submission must run on the thread that owns the GPU context/queue.
// In Unreal Engine that is the Render Thread, while gameplay ticking, actor
// components, and camera updates run on the Game Thread.
//
// Unreal's primary mechanism for dispatching rendering work is
// ENQUEUE_RENDER_COMMAND, which schedules a command onto the Render Thread.
// This file provides the bridge: C++ gameplay code stages a frame on the
// Game Thread, enqueues a render command carrying the slot index, and
// NexVR_Unreal_ProcessRenderCommand executes the submission on the Render Thread.
//
// ===========================================================================
// GRAPHICS OWNERSHIP & ZERO VENDOR COUPLING
// ===========================================================================
//
// Like the Unity bridge, this adapter does not vendor private Unreal Engine RHI
// headers (such as D3D11RHI.h / D3D12RHI.h). The ID3D11Texture2D* / ID3D12Resource*
// retrieved via FRHITexture->GetNativeResource() already knows its device via
// GetDevice(). The bridge consumes raw DirectX pointers, keeping it strictly
// decoupled from Unreal Engine minor versions.

#include "nexvr_unreal_bridge.h"

#include <d3d11.h>
#include <d3d12.h>

#include <array>
#include <atomic>
#include <cstdint>

namespace {

enum class SlotState : int {
    Free = 0,     /**< Available for the Game Thread to claim. */
    Filling = 1,  /**< The Game Thread has claimed it and is writing fields. */
    Ready = 2,    /**< Fully populated; the Render Thread may consume it. */
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
    NexVR_DepthRange depthRange{NEXVR_DEPTH_RANGE_REVERSED};
    bool hasDepth{false};

    std::atomic<SlotState> state{SlotState::Free};
};

constexpr int kRingSize = 8;
std::array<StagedFrame, kRingSize> g_ring;

std::atomic<NexVR_Session> g_session{nullptr};

std::atomic<uint32_t> g_submitted{0};
std::atomic<uint32_t> g_dropped{0};
std::atomic<uint32_t> g_failed{0};
std::atomic<int32_t> g_lastResult{NEXVR_SUCCESS};

}  // namespace

extern "C" {

__declspec(dllexport) void __stdcall NexVR_Unreal_Attach(NexVR_Session session) {
    g_session.store(session, std::memory_order_release);
    g_submitted.store(0, std::memory_order_relaxed);
    g_dropped.store(0, std::memory_order_relaxed);
    g_failed.store(0, std::memory_order_relaxed);
    g_lastResult.store(NEXVR_SUCCESS, std::memory_order_relaxed);
}

__declspec(dllexport) void __stdcall NexVR_Unreal_Detach() {
    g_session.store(nullptr, std::memory_order_release);
    for (StagedFrame& frame : g_ring) {
        frame.state.store(SlotState::Free, std::memory_order_release);
    }
}

__declspec(dllexport) int __stdcall NexVR_Unreal_StageFrameWithDepth(
    uint64_t token, void* colorTexture, void* depthTexture, float nearZ, float farZ, int depthRange) {
    if (colorTexture == nullptr) {
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

        frame.api = BridgeApi::D3D11;
        frame.token = token;
        frame.colorResource = colorTexture;
        frame.depthResource = depthTexture;
        frame.colorState = 0;
        frame.depthState = 0;
        frame.nearZ = nearZ;
        frame.farZ = farZ;
        frame.depthRange = (depthRange == 0) ? NEXVR_DEPTH_RANGE_ZERO_TO_ONE : NEXVR_DEPTH_RANGE_REVERSED;
        frame.hasDepth = (depthTexture != nullptr);

        // Release store so all struct fields are published before state becomes Ready.
        frame.state.store(SlotState::Ready, std::memory_order_release);
        return slot;
    }

    // All slots in flight: the Render Thread is behind. Do not stall the Game Thread.
    return -1;
}

__declspec(dllexport) int __stdcall NexVR_Unreal_StageFrame(uint64_t token, void* colorTexture) {
    return NexVR_Unreal_StageFrameWithDepth(token, colorTexture, nullptr, 0.0f, 0.0f, 0);
}

__declspec(dllexport) int __stdcall NexVR_Unreal_StageFrameWithDepthDX12(
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
        frame.depthRange = (depthRange == 0) ? NEXVR_DEPTH_RANGE_ZERO_TO_ONE : NEXVR_DEPTH_RANGE_REVERSED;
        frame.hasDepth = (depthResource != nullptr);

        frame.state.store(SlotState::Ready, std::memory_order_release);
        return slot;
    }

    return -1;
}

__declspec(dllexport) int __stdcall NexVR_Unreal_StageFrameDX12(
    uint64_t token, void* colorResource, uint32_t colorState) {
    return NexVR_Unreal_StageFrameWithDepthDX12(token, colorResource, colorState, nullptr, 0, 0.0f, 0.0f, 0);
}

__declspec(dllexport) void __stdcall NexVR_Unreal_ProcessRenderCommand(int slot) {
    if (slot < 0 || slot >= kRingSize) {
        return;
    }

    StagedFrame& frame = g_ring[static_cast<size_t>(slot)];

    if (frame.state.load(std::memory_order_acquire) != SlotState::Ready) {
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
        g_dropped.fetch_add(1, std::memory_order_relaxed);
    } else if (NEXVR_FAILED(result)) {
        g_failed.fetch_add(1, std::memory_order_relaxed);
    } else {
        g_submitted.fetch_add(1, std::memory_order_relaxed);
    }

    // Free the slot once the Render Thread is finished.
    frame.state.store(SlotState::Free, std::memory_order_release);
}

__declspec(dllexport) int __stdcall NexVR_Unreal_SupportsDepthSubmission() {
    NexVR_Session session = g_session.load(std::memory_order_acquire);
    if (session == nullptr) {
        return 0;
    }
    return NexVR_SupportsDepthSubmission(session) ? 1 : 0;
}

__declspec(dllexport) int __stdcall NexVR_Unreal_GetDeviceFromTexture(
    void* texture, void** outDevice, void** outContext) {
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

    *outDevice = device;
    *outContext = context;
    return 1;
}

__declspec(dllexport) void __stdcall NexVR_Unreal_ReleaseDeviceHandles(void* device, void* context) {
    if (context != nullptr) {
        static_cast<ID3D11DeviceContext*>(context)->Release();
    }
    if (device != nullptr) {
        static_cast<ID3D11Device*>(device)->Release();
    }
}

__declspec(dllexport) int __stdcall NexVR_Unreal_GetDeviceFromResourceDX12(
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

__declspec(dllexport) void __stdcall NexVR_Unreal_ReleaseDeviceHandleDX12(void* device) {
    if (device != nullptr) {
        static_cast<ID3D12Device*>(device)->Release();
    }
}

__declspec(dllexport) void __stdcall NexVR_Unreal_GetStats(
    uint32_t* submitted, uint32_t* dropped, uint32_t* failed, int32_t* lastResult) {
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

__declspec(dllexport) int __stdcall NexVR_Unreal_SyncInput() {
    NexVR_Session session = g_session.load(std::memory_order_acquire);
    if (session == nullptr) {
        return 0;
    }
    return NEXVR_SUCCEEDED(NexVR_SyncInput(session)) ? 1 : 0;
}

__declspec(dllexport) int __stdcall NexVR_Unreal_GetControllerState(int hand, NexVR_ControllerState* outState) {
    NexVR_Session session = g_session.load(std::memory_order_acquire);
    if (session == nullptr || outState == nullptr) {
        return 0;
    }
    const NexVR_Hand h = (hand == 0) ? NEXVR_HAND_LEFT : NEXVR_HAND_RIGHT;
    return NEXVR_SUCCEEDED(NexVR_GetControllerState(session, h, outState)) ? 1 : 0;
}

__declspec(dllexport) int __stdcall NexVR_Unreal_TriggerHaptic(int hand, float durationMs, float frequencyHz, float amplitude) {
    NexVR_Session session = g_session.load(std::memory_order_acquire);
    if (session == nullptr) {
        return 0;
    }
    const NexVR_Hand h = (hand == 0) ? NEXVR_HAND_LEFT : NEXVR_HAND_RIGHT;
    NexVR_HapticFeedback haptic{};
    haptic.structSize = sizeof(NexVR_HapticFeedback);
    haptic.durationMs = durationMs;
    haptic.frequencyHz = frequencyHz;
    haptic.amplitude = amplitude;
    return NEXVR_SUCCEEDED(NexVR_TriggerHaptic(session, h, &haptic)) ? 1 : 0;
}

__declspec(dllexport) int __stdcall NexVR_Unreal_StopHaptic(int hand) {
    NexVR_Session session = g_session.load(std::memory_order_acquire);
    if (session == nullptr) {
        return 0;
    }
    const NexVR_Hand h = (hand == 0) ? NEXVR_HAND_LEFT : NEXVR_HAND_RIGHT;
    return NEXVR_SUCCEEDED(NexVR_StopHaptic(session, h)) ? 1 : 0;
}

// ===========================================================================
// Stereix Enterprise Unreal Bridge C ABI Forwarders
// ===========================================================================

__declspec(dllexport) void __stdcall Stereix_Unreal_Attach(NexVR_Session session) {
    NexVR_Unreal_Attach(session);
}

__declspec(dllexport) void __stdcall Stereix_Unreal_Detach() {
    NexVR_Unreal_Detach();
}

__declspec(dllexport) int __stdcall Stereix_Unreal_StageFrameWithDepth(
    uint64_t token, void* colorTexture, void* depthTexture, float nearZ, float farZ, int depthRange) {
    return NexVR_Unreal_StageFrameWithDepth(token, colorTexture, depthTexture, nearZ, farZ, depthRange);
}

__declspec(dllexport) int __stdcall Stereix_Unreal_StageFrame(uint64_t token, void* colorTexture) {
    return NexVR_Unreal_StageFrame(token, colorTexture);
}

__declspec(dllexport) int __stdcall Stereix_Unreal_StageFrameWithDepthDX12(
    uint64_t token, void* colorResource, uint32_t colorState,
    void* depthResource, uint32_t depthState,
    float nearZ, float farZ, int depthRange) {
    return NexVR_Unreal_StageFrameWithDepthDX12(token, colorResource, colorState, depthResource, depthState, nearZ, farZ, depthRange);
}

__declspec(dllexport) int __stdcall Stereix_Unreal_StageFrameDX12(
    uint64_t token, void* colorResource, uint32_t colorState) {
    return NexVR_Unreal_StageFrameDX12(token, colorResource, colorState);
}

__declspec(dllexport) void __stdcall Stereix_Unreal_ProcessRenderCommand(int slot) {
    NexVR_Unreal_ProcessRenderCommand(slot);
}

__declspec(dllexport) int __stdcall Stereix_Unreal_SupportsDepthSubmission() {
    return NexVR_Unreal_SupportsDepthSubmission();
}

__declspec(dllexport) int __stdcall Stereix_Unreal_GetDeviceFromTexture(
    void* texture, void** outDevice, void** outContext) {
    return NexVR_Unreal_GetDeviceFromTexture(texture, outDevice, outContext);
}

__declspec(dllexport) void __stdcall Stereix_Unreal_ReleaseDeviceHandles(void* device, void* context) {
    NexVR_Unreal_ReleaseDeviceHandles(device, context);
}

__declspec(dllexport) int __stdcall Stereix_Unreal_GetDeviceFromResourceDX12(
    void* resource, void** outDevice) {
    return NexVR_Unreal_GetDeviceFromResourceDX12(resource, outDevice);
}

__declspec(dllexport) void __stdcall Stereix_Unreal_ReleaseDeviceHandleDX12(void* device) {
    NexVR_Unreal_ReleaseDeviceHandleDX12(device);
}

__declspec(dllexport) void __stdcall Stereix_Unreal_GetStats(
    uint32_t* submitted, uint32_t* dropped, uint32_t* failed, int32_t* lastResult) {
    NexVR_Unreal_GetStats(submitted, dropped, failed, lastResult);
}

__declspec(dllexport) int __stdcall Stereix_Unreal_SyncInput() {
    return NexVR_Unreal_SyncInput();
}

__declspec(dllexport) int __stdcall Stereix_Unreal_GetControllerState(int hand, NexVR_ControllerState* outState) {
    return NexVR_Unreal_GetControllerState(hand, outState);
}

__declspec(dllexport) int __stdcall Stereix_Unreal_TriggerHaptic(int hand, float durationMs, float frequencyHz, float amplitude) {
    return NexVR_Unreal_TriggerHaptic(hand, durationMs, frequencyHz, amplitude);
}

__declspec(dllexport) int __stdcall Stereix_Unreal_StopHaptic(int hand) {
    return NexVR_Unreal_StopHaptic(hand);
}

}  // extern "C"
