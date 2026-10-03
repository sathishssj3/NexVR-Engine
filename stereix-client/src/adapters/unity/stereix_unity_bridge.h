#pragma once

#include "stereix_sdk.h"

#ifdef __cplusplus
extern "C" {
#endif

using UnityRenderingEvent = void(__stdcall*)(int eventId);

#ifndef STEREIX_UNITY_API
#ifdef STEREIX_UNITY_BUILD_DLL
#define STEREIX_UNITY_API __declspec(dllexport)
#else
#define STEREIX_UNITY_API __declspec(dllimport)
#endif
#endif

STEREIX_UNITY_API UnityRenderingEvent __stdcall Stereix_Unity_GetRenderEventFunc();
STEREIX_UNITY_API void __stdcall Stereix_Unity_Attach(Stereix_Session session);
STEREIX_UNITY_API void __stdcall Stereix_Unity_Detach();

// --- D3D11 Staging ---
STEREIX_UNITY_API int __stdcall Stereix_Unity_StageFrameWithDepth(
    uint64_t token, void* texture, void* depthTexture, float nearZ, float farZ, int depthRange);
STEREIX_UNITY_API int __stdcall Stereix_Unity_StageFrame(uint64_t token, void* texture);

// --- D3D12 Staging ---
STEREIX_UNITY_API int __stdcall Stereix_Unity_StageFrameWithDepthDX12(
    uint64_t token, void* colorResource, uint32_t colorState,
    void* depthResource, uint32_t depthState,
    float nearZ, float farZ, int depthRange);
STEREIX_UNITY_API int __stdcall Stereix_Unity_StageFrameDX12(
    uint64_t token, void* colorResource, uint32_t colorState);

STEREIX_UNITY_API int __stdcall Stereix_Unity_SupportsDepthSubmission();

// --- Device Recovery ---
STEREIX_UNITY_API int __stdcall Stereix_Unity_GetDeviceFromTexture(void* texture, void** outDevice, void** outContext);
STEREIX_UNITY_API void __stdcall Stereix_Unity_ReleaseDeviceHandles(void* device, void* context);

STEREIX_UNITY_API int __stdcall Stereix_Unity_GetDeviceFromResourceDX12(void* resource, void** outDevice);
STEREIX_UNITY_API void __stdcall Stereix_Unity_ReleaseDeviceHandleDX12(void* device);

// --- Diagnostics ---
STEREIX_UNITY_API void __stdcall Stereix_Unity_GetStats(uint32_t* submitted, uint32_t* dropped,
                                                       uint32_t* failed, int32_t* lastResult);

// --- Input and Haptics ---
STEREIX_UNITY_API int __stdcall Stereix_Unity_SyncInput();
STEREIX_UNITY_API int __stdcall Stereix_Unity_GetControllerState(
    Stereix_Hand hand, Stereix_ControllerState* outState);
STEREIX_UNITY_API int __stdcall Stereix_Unity_TriggerHaptic(
    Stereix_Hand hand, const Stereix_HapticFeedback* haptic);
STEREIX_UNITY_API int __stdcall Stereix_Unity_StopHaptic(Stereix_Hand hand);

#ifdef __cplusplus
}
#endif
