#pragma once

// The NexVR Unreal Engine Native RHI Bridge C ABI.
//
// Bridges Unreal Engine's Game Thread and Render Thread (via ENQUEUE_RENDER_COMMAND)
// to NexVR's D3D11 frame submission pipeline.
//
// Threading contract:
//   - NexVR_Unreal_StageFrame / NexVR_Unreal_StageFrameWithDepth: GAME THREAD only.
//   - NexVR_Unreal_ProcessRenderCommand: RENDER THREAD only (inside ENQUEUE_RENDER_COMMAND).
//   - NexVR_Unreal_Detach: GAME THREAD during module shutdown, BEFORE NexVR_Shutdown.

#include "nexvr_sdk.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NEXVR_UNREAL_API
#  ifdef NEXVR_UNREAL_BUILD_DLL
#    define NEXVR_UNREAL_API __declspec(dllexport)
#  else
#    define NEXVR_UNREAL_API __declspec(dllimport)
#  endif
#endif

/**
 * Hand the bridge an active session created during module startup.
 * Resets diagnostic counters. Call from Game Thread.
 */
NEXVR_UNREAL_API void __stdcall NexVR_Unreal_Attach(NexVR_Session session);

/**
 * Stop the Render Thread from using the session.
 * Resets all ring slots to Free. Call from Game Thread BEFORE NexVR_Shutdown.
 */
NEXVR_UNREAL_API void __stdcall NexVR_Unreal_Detach();

/**
 * Stage a frame with an optional depth buffer from the GAME THREAD.
 *
 * @param token         Opaque token from NexVR_WaitFrame on this thread.
 * @param colorTexture  ID3D11Texture2D* from Unreal FRHITexture2D->GetNativeResource().
 * @param depthTexture  ID3D11Texture2D* from Unreal SceneDepthTexture->GetNativeResource(), or nullptr.
 * @param nearZ         Near clip plane in meters (positive).
 * @param farZ          Far clip plane in meters (positive, or INFINITY).
 * @param depthRange    0 = ZeroToOne, 1 = Reversed (Unreal Engine default is 1).
 * @return              The ring buffer slot index (0..7) to pass into ENQUEUE_RENDER_COMMAND,
 *                      or -1 if the Render Thread is behind and every slot is full.
 */
NEXVR_UNREAL_API int __stdcall NexVR_Unreal_StageFrameWithDepth(
    uint64_t token, void* colorTexture, void* depthTexture, float nearZ, float farZ, int depthRange);

/**
 * Backwards-compatible color-only staging forwarder (DEC-020).
 * Calls NexVR_Unreal_StageFrameWithDepth with null depth and 0 ranges.
 */
NEXVR_UNREAL_API int __stdcall NexVR_Unreal_StageFrame(uint64_t token, void* colorTexture);

/**
 * Stage a Direct3D 12 frame with an optional depth buffer from Unreal's GAME THREAD.
 *
 * @param token         Opaque token from NexVR_WaitFrame on this thread.
 * @param colorResource ID3D12Resource* from Unreal FRHITexture2D->GetNativeResource().
 * @param colorState    D3D12_RESOURCE_STATES of colorResource (e.g. RENDER_TARGET).
 * @param depthResource ID3D12Resource* from Unreal SceneDepthTexture, or nullptr.
 * @param depthState    D3D12_RESOURCE_STATES of depthResource (e.g. DEPTH_READ).
 * @param nearZ         Near clip plane in meters (positive).
 * @param farZ          Far clip plane in meters (positive, or INFINITY).
 * @param depthRange    0 = ZeroToOne, 1 = Reversed (Unreal Engine 5 default is 1).
 * @return              Slot index (0..7) to pass into ENQUEUE_RENDER_COMMAND, or -1 if full.
 */
NEXVR_UNREAL_API int __stdcall NexVR_Unreal_StageFrameWithDepthDX12(
    uint64_t token, void* colorResource, uint32_t colorState,
    void* depthResource, uint32_t depthState,
    float nearZ, float farZ, int depthRange);

/**
 * Color-only D3D12 staging forwarder (DEC-020).
 */
NEXVR_UNREAL_API int __stdcall NexVR_Unreal_StageFrameDX12(
    uint64_t token, void* colorResource, uint32_t colorState);

/**
 * Process a staged frame on Unreal's RENDER THREAD.
 *
 * Call this inside an ENQUEUE_RENDER_COMMAND lambda on the Render Thread.
 * Automatically dispatches to D3D11 or D3D12 SDK submission paths based on
 * how the frame was staged.
 *
 * @param slot  The slot index returned by NexVR_Unreal_StageFrame*.
 */
NEXVR_UNREAL_API void __stdcall NexVR_Unreal_ProcessRenderCommand(int slot);

/**
 * Query whether the current attached session supports depth submission (XR_KHR_composition_layer_depth).
 * Returns 1 if supported and usable, 0 if not or session is null.
 */
NEXVR_UNREAL_API int __stdcall NexVR_Unreal_SupportsDepthSubmission();

/**
 * Recover the ID3D11Device and immediate ID3D11DeviceContext from any texture Unreal created on it.
 * Enables binding without vendor headers. Both handles are AddRef'd and must be released
 * via NexVR_Unreal_ReleaseDeviceHandles.
 */
NEXVR_UNREAL_API int __stdcall NexVR_Unreal_GetDeviceFromTexture(
    void* texture, void** outDevice, void** outContext);

/**
 * Releases handles acquired by NexVR_Unreal_GetDeviceFromTexture.
 */
NEXVR_UNREAL_API void __stdcall NexVR_Unreal_ReleaseDeviceHandles(void* device, void* context);

/**
 * Recover the ID3D12Device from any resource Unreal created on it.
 * Calls AddRef on the device; caller must release via NexVR_Unreal_ReleaseDeviceHandleDX12.
 */
NEXVR_UNREAL_API int __stdcall NexVR_Unreal_GetDeviceFromResourceDX12(
    void* resource, void** outDevice);

/**
 * Releases device handle acquired by NexVR_Unreal_GetDeviceFromResourceDX12.
 */
NEXVR_UNREAL_API void __stdcall NexVR_Unreal_ReleaseDeviceHandleDX12(void* device);

/**
 * Diagnostic counters for telemetry and monitoring.
 */
NEXVR_UNREAL_API void __stdcall NexVR_Unreal_GetStats(
    uint32_t* submitted, uint32_t* dropped, uint32_t* failed, int32_t* lastResult);

// --- 6DOF Controller Input & Haptics ---
/**
 * Synchronize input actions from the OpenXR runtime. Call from Game Thread.
 */
NEXVR_UNREAL_API int __stdcall NexVR_Unreal_SyncInput();

/**
 * Read 6DOF tracking and input state for a controller (0 = Left, 1 = Right).
 */
NEXVR_UNREAL_API int __stdcall NexVR_Unreal_GetControllerState(int hand, NexVR_ControllerState* outState);

/**
 * Trigger haptic vibration on a controller (0 = Left, 1 = Right).
 */
NEXVR_UNREAL_API int __stdcall NexVR_Unreal_TriggerHaptic(int hand, float durationMs, float frequencyHz, float amplitude);

/**
 * Immediately stop haptic vibration on a controller.
 */
NEXVR_UNREAL_API int __stdcall NexVR_Unreal_StopHaptic(int hand);

// ===========================================================================
// Stereix Enterprise Unreal Bridge C ABI Exports
// ===========================================================================
#define STEREIX_UNREAL_API NEXVR_UNREAL_API

STEREIX_UNREAL_API void __stdcall Stereix_Unreal_Attach(NexVR_Session session);
STEREIX_UNREAL_API void __stdcall Stereix_Unreal_Detach();

STEREIX_UNREAL_API int __stdcall Stereix_Unreal_StageFrameWithDepth(
    uint64_t token, void* colorTexture, void* depthTexture, float nearZ, float farZ, int depthRange);
STEREIX_UNREAL_API int __stdcall Stereix_Unreal_StageFrame(uint64_t token, void* colorTexture);

STEREIX_UNREAL_API int __stdcall Stereix_Unreal_StageFrameWithDepthDX12(
    uint64_t token, void* colorResource, uint32_t colorState,
    void* depthResource, uint32_t depthState,
    float nearZ, float farZ, int depthRange);
STEREIX_UNREAL_API int __stdcall Stereix_Unreal_StageFrameDX12(
    uint64_t token, void* colorResource, uint32_t colorState);

STEREIX_UNREAL_API void __stdcall Stereix_Unreal_ProcessRenderCommand(int slot);
STEREIX_UNREAL_API int __stdcall Stereix_Unreal_SupportsDepthSubmission();

STEREIX_UNREAL_API int __stdcall Stereix_Unreal_GetDeviceFromTexture(
    void* texture, void** outDevice, void** outContext);
STEREIX_UNREAL_API void __stdcall Stereix_Unreal_ReleaseDeviceHandles(void* device, void* context);

STEREIX_UNREAL_API int __stdcall Stereix_Unreal_GetDeviceFromResourceDX12(
    void* resource, void** outDevice);
STEREIX_UNREAL_API void __stdcall Stereix_Unreal_ReleaseDeviceHandleDX12(void* device);

STEREIX_UNREAL_API void __stdcall Stereix_Unreal_GetStats(
    uint32_t* submitted, uint32_t* dropped, uint32_t* failed, int32_t* lastResult);

STEREIX_UNREAL_API int __stdcall Stereix_Unreal_SyncInput();
STEREIX_UNREAL_API int __stdcall Stereix_Unreal_GetControllerState(int hand, NexVR_ControllerState* outState);
STEREIX_UNREAL_API int __stdcall Stereix_Unreal_TriggerHaptic(int hand, float durationMs, float frequencyHz, float amplitude);
STEREIX_UNREAL_API int __stdcall Stereix_Unreal_StopHaptic(int hand);

#ifdef __cplusplus
}
#endif
