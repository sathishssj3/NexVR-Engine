#pragma once

#include "nexvr_sdk.h"

#ifdef __cplusplus
extern "C" {
#endif

using UnityRenderingEvent = void(__stdcall*)(int eventId);

#ifndef NEXVR_UNITY_API
#ifdef NEXVR_UNITY_BUILD_DLL
#define NEXVR_UNITY_API __declspec(dllexport)
#else
#define NEXVR_UNITY_API __declspec(dllimport)
#endif
#endif

NEXVR_UNITY_API UnityRenderingEvent __stdcall NexVR_Unity_GetRenderEventFunc();
NEXVR_UNITY_API void __stdcall NexVR_Unity_Attach(NexVR_Session session);
NEXVR_UNITY_API void __stdcall NexVR_Unity_Detach();

// --- D3D11 Staging ---
NEXVR_UNITY_API int __stdcall NexVR_Unity_StageFrameWithDepth(
    uint64_t token, void* texture, void* depthTexture, float nearZ, float farZ, int depthRange);
NEXVR_UNITY_API int __stdcall NexVR_Unity_StageFrame(uint64_t token, void* texture);

// --- D3D12 Staging ---
NEXVR_UNITY_API int __stdcall NexVR_Unity_StageFrameWithDepthDX12(
    uint64_t token, void* colorResource, uint32_t colorState,
    void* depthResource, uint32_t depthState,
    float nearZ, float farZ, int depthRange);
NEXVR_UNITY_API int __stdcall NexVR_Unity_StageFrameDX12(
    uint64_t token, void* colorResource, uint32_t colorState);

NEXVR_UNITY_API int __stdcall NexVR_Unity_SupportsDepthSubmission();

// --- Device Recovery ---
NEXVR_UNITY_API int __stdcall NexVR_Unity_GetDeviceFromTexture(void* texture, void** outDevice,
                                                              void** outContext);
NEXVR_UNITY_API void __stdcall NexVR_Unity_ReleaseDeviceHandles(void* device, void* context);

NEXVR_UNITY_API int __stdcall NexVR_Unity_GetDeviceFromResourceDX12(void* resource, void** outDevice);
NEXVR_UNITY_API void __stdcall NexVR_Unity_ReleaseDeviceHandleDX12(void* device);

// --- Diagnostics ---
NEXVR_UNITY_API void __stdcall NexVR_Unity_GetStats(uint32_t* submitted, uint32_t* dropped,
                                                    uint32_t* failed, int32_t* lastResult);

// --- Input and Haptics ---
NEXVR_UNITY_API int __stdcall NexVR_Unity_SyncInput();
NEXVR_UNITY_API int __stdcall NexVR_Unity_GetControllerState(
    NexVR_Hand hand, NexVR_ControllerState* outState);
NEXVR_UNITY_API int __stdcall NexVR_Unity_TriggerHaptic(
    NexVR_Hand hand, const NexVR_HapticFeedback* haptic);
NEXVR_UNITY_API int __stdcall NexVR_Unity_StopHaptic(NexVR_Hand hand);

#ifdef __cplusplus
}
#endif
