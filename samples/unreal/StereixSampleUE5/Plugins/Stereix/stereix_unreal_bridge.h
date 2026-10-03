#pragma once

// The Stereix Unreal Engine Native RHI Bridge C ABI.
//
// Bridges Unreal Engine's Game Thread and Render Thread (via ENQUEUE_RENDER_COMMAND)
// to Stereix's D3D11 & D3D12 frame submission pipeline.
//
// Threading contract:
//   - Stereix_Unreal_StageFrame / Stereix_Unreal_StageFrameWithDepth: GAME THREAD only.
//   - Stereix_Unreal_ProcessRenderCommand: RENDER THREAD only (inside ENQUEUE_RENDER_COMMAND).
//   - Stereix_Unreal_Detach: GAME THREAD during module shutdown, BEFORE Stereix_Shutdown.

#include "stereix_sdk.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef STEREIX_UNREAL_API
#  ifdef STEREIX_UNREAL_BUILD_DLL
#    define STEREIX_UNREAL_API __declspec(dllexport)
#  else
#    define STEREIX_UNREAL_API __declspec(dllimport)
#  endif
#endif

/**
 * Hand the bridge an active session created during module startup.
 * Resets diagnostic counters. Call from Game Thread.
 */
STEREIX_UNREAL_API void __stdcall Stereix_Unreal_Attach(Stereix_Session session);

/**
 * Stop the Render Thread from using the session.
 * Resets all ring slots to Free. Call from Game Thread BEFORE Stereix_Shutdown.
 */
STEREIX_UNREAL_API void __stdcall Stereix_Unreal_Detach();

/**
 * Stage a frame with an optional depth buffer from the GAME THREAD (D3D11).
 */
STEREIX_UNREAL_API int __stdcall Stereix_Unreal_StageFrameWithDepth(
    uint64_t token, void* colorTexture, void* depthTexture, float nearZ, float farZ, int depthRange);

STEREIX_UNREAL_API int __stdcall Stereix_Unreal_StageFrame(uint64_t token, void* colorTexture);

/**
 * Stage a frame with an optional depth buffer from the GAME THREAD (D3D12).
 */
STEREIX_UNREAL_API int __stdcall Stereix_Unreal_StageFrameWithDepthDX12(
    uint64_t token, void* colorResource, uint32_t colorState,
    void* depthResource, uint32_t depthState,
    float nearZ, float farZ, int depthRange);

STEREIX_UNREAL_API int __stdcall Stereix_Unreal_StageFrameDX12(
    uint64_t token, void* colorResource, uint32_t colorState);

/**
 * Execute submission on the RENDER THREAD inside ENQUEUE_RENDER_COMMAND.
 */
STEREIX_UNREAL_API void __stdcall Stereix_Unreal_ProcessRenderCommand(int slot);

/**
 * Check if the active session supports depth layer submission.
 */
STEREIX_UNREAL_API int __stdcall Stereix_Unreal_SupportsDepthSubmission();

/**
 * Device recovery helpers.
 */
STEREIX_UNREAL_API int __stdcall Stereix_Unreal_GetDeviceFromTexture(
    void* texture, void** outDevice, void** outContext);
STEREIX_UNREAL_API void __stdcall Stereix_Unreal_ReleaseDeviceHandles(void* device, void* context);

STEREIX_UNREAL_API int __stdcall Stereix_Unreal_GetDeviceFromResourceDX12(
    void* resource, void** outDevice);
STEREIX_UNREAL_API void __stdcall Stereix_Unreal_ReleaseDeviceHandleDX12(void* device);

/**
 * Diagnostic stats.
 */
STEREIX_UNREAL_API void __stdcall Stereix_Unreal_GetStats(
    uint32_t* submitted, uint32_t* dropped, uint32_t* failed, int32_t* lastResult);

/**
 * 6DOF Controller Input & Haptics.
 */
STEREIX_UNREAL_API int __stdcall Stereix_Unreal_SyncInput();
STEREIX_UNREAL_API int __stdcall Stereix_Unreal_GetControllerState(int hand, Stereix_ControllerState* outState);
STEREIX_UNREAL_API int __stdcall Stereix_Unreal_TriggerHaptic(int hand, float durationMs, float frequencyHz, float amplitude);
STEREIX_UNREAL_API int __stdcall Stereix_Unreal_StopHaptic(int hand);

#ifdef __cplusplus
}
#endif
