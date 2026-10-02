/*===========================================================================
  stereix_sdk.h — Stereix Engine B2B SDK, Official C ABI
  Copyright (c) 2026 Mesmeran Lab. All rights reserved.
  ---------------------------------------------------------------------------
  DirectX 11, DirectX 12, and Vulkan real-time VR injection & spatial
  rendering runtime for enterprise game studios and simulation developers.
===========================================================================*/

#ifndef STEREIX_SDK_H
#define STEREIX_SDK_H

#include "nexvr_sdk.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- Versioning ----------------------------------------------------------- */
#define STEREIX_SDK_VERSION_MAJOR NEXVR_SDK_VERSION_MAJOR
#define STEREIX_SDK_VERSION_MINOR NEXVR_SDK_VERSION_MINOR
#define STEREIX_SDK_VERSION_PATCH NEXVR_SDK_VERSION_PATCH

#define STEREIX_MAKE_VERSION(ma, mi, pa) NEXVR_MAKE_VERSION(ma, mi, pa)
#define STEREIX_SDK_VERSION NEXVR_SDK_VERSION

#ifdef STEREIX_SDK_IMPLEMENTATION
#  define STEREIX_API __declspec(dllexport)
#else
#  define STEREIX_API __declspec(dllimport)
#endif

#define STEREIX_CALL __stdcall

/* --- Result Types & Error Codes ------------------------------------------- */
typedef NexVR_Result Stereix_Result;

#define STEREIX_SUCCESS                       NEXVR_SUCCESS
#define STEREIX_SESSION_EXITING               NEXVR_SESSION_EXITING
#define STEREIX_FRAME_SKIPPED                 NEXVR_FRAME_SKIPPED

#define STEREIX_ERROR_RUNTIME_UNAVAILABLE     NEXVR_ERROR_RUNTIME_UNAVAILABLE
#define STEREIX_ERROR_NO_HEADSET              NEXVR_ERROR_NO_HEADSET
#define STEREIX_ERROR_REQUIREMENTS_NOT_QUERIED NEXVR_ERROR_REQUIREMENTS_NOT_QUERIED
#define STEREIX_ERROR_WRONG_ADAPTER           NEXVR_ERROR_WRONG_ADAPTER
#define STEREIX_ERROR_DIMENSION_MISMATCH      NEXVR_ERROR_DIMENSION_MISMATCH
#define STEREIX_ERROR_INVALID_FORMAT          NEXVR_ERROR_INVALID_FORMAT
#define STEREIX_ERROR_NOT_IMMEDIATE_CONTEXT   NEXVR_ERROR_NOT_IMMEDIATE_CONTEXT
#define STEREIX_ERROR_INVALID_COMMAND_QUEUE   NEXVR_ERROR_INVALID_COMMAND_QUEUE
#define STEREIX_ERROR_INVALID_VULKAN_QUEUE    NEXVR_ERROR_INVALID_VULKAN_QUEUE
#define STEREIX_ERROR_COLOR_SPACE_UNSPECIFIED NEXVR_ERROR_COLOR_SPACE_UNSPECIFIED
#define STEREIX_ERROR_MULTITHREAD_UNAVAILABLE NEXVR_ERROR_MULTITHREAD_UNAVAILABLE
#define STEREIX_ERROR_INVALID_FRAME_TOKEN     NEXVR_ERROR_INVALID_FRAME_TOKEN
#define STEREIX_ERROR_SWAPCHAIN_TIMEOUT       NEXVR_ERROR_SWAPCHAIN_TIMEOUT
#define STEREIX_ERROR_INVALID_ARGUMENT        NEXVR_ERROR_INVALID_ARGUMENT
#define STEREIX_ERROR_INVALID_SESSION         NEXVR_ERROR_INVALID_SESSION
#define STEREIX_ERROR_RUNTIME_FAILURE         NEXVR_ERROR_RUNTIME_FAILURE

#define STEREIX_SUCCEEDED(r) NEXVR_SUCCEEDED(r)
#define STEREIX_FAILED(r)    NEXVR_FAILED(r)

/* --- Colour Space & Depth Formats ----------------------------------------- */
typedef NexVR_ColorSpace Stereix_ColorSpace;
#define STEREIX_COLOR_SPACE_UNSPECIFIED   NEXVR_COLOR_SPACE_UNSPECIFIED
#define STEREIX_COLOR_SPACE_SRGB_ENCODED  NEXVR_COLOR_SPACE_SRGB_ENCODED
#define STEREIX_COLOR_SPACE_LINEAR        NEXVR_COLOR_SPACE_LINEAR

typedef NexVR_DepthRange Stereix_DepthRange;
#define STEREIX_DEPTH_RANGE_STANDARD      NEXVR_DEPTH_RANGE_STANDARD
#define STEREIX_DEPTH_RANGE_REVERSED      NEXVR_DEPTH_RANGE_REVERSED

typedef NexVR_Hand Stereix_Hand;
#define STEREIX_HAND_LEFT                 NEXVR_HAND_LEFT
#define STEREIX_HAND_RIGHT                NEXVR_HAND_RIGHT

typedef NexVR_TrackingOrigin Stereix_TrackingOrigin;
#define STEREIX_TRACKING_ORIGIN_LOCAL     NEXVR_TRACKING_ORIGIN_LOCAL
#define STEREIX_TRACKING_ORIGIN_STAGE     NEXVR_TRACKING_ORIGIN_STAGE

typedef NexVR_GraphicsApi Stereix_GraphicsApi;

/* --- Handles & Structures ------------------------------------------------- */
typedef NexVR_Session Stereix_Session;

typedef NexVR_GraphicsRequirements        Stereix_GraphicsRequirements;
typedef NexVR_GraphicsRequirementsDX12    Stereix_GraphicsRequirementsDX12;
typedef NexVR_GraphicsRequirementsVulkan  Stereix_GraphicsRequirementsVulkan;

typedef NexVR_InitializeDX11Info          Stereix_InitializeDX11Info;
typedef NexVR_InitializeDX12Info          Stereix_InitializeDX12Info;
typedef NexVR_InitializeVulkanInfo        Stereix_InitializeVulkanInfo;

typedef NexVR_DepthInfoDX11               Stereix_DepthInfoDX11;
typedef NexVR_DepthInfoDX12               Stereix_DepthInfoDX12;
typedef NexVR_DepthInfoVulkan             Stereix_DepthInfoVulkan;

typedef NexVR_Fov                         Stereix_Fov;
typedef NexVR_Pose                        Stereix_Pose;
typedef NexVR_View                        Stereix_View;
typedef NexVR_FrameState                  Stereix_FrameState;
typedef NexVR_TrackingState               Stereix_TrackingState;
typedef NexVR_ControllerState             Stereix_ControllerState;
typedef NexVR_HapticFeedback              Stereix_HapticFeedback;

/* --- Core Function Bindings & Inlines ------------------------------------ */
static inline Stereix_Result Stereix_GetGraphicsRequirements(Stereix_GraphicsRequirements* requirements) {
    return NexVR_GetGraphicsRequirements(requirements);
}

static inline Stereix_Result Stereix_InitializeDX11(const Stereix_InitializeDX11Info* info, Stereix_Session* outSession) {
    return NexVR_InitializeDX11(info, outSession);
}

static inline Stereix_Result Stereix_GetGraphicsRequirementsDX12(Stereix_GraphicsRequirementsDX12* requirements) {
    return NexVR_GetGraphicsRequirementsDX12(requirements);
}

static inline Stereix_Result Stereix_InitializeDX12(const Stereix_InitializeDX12Info* info, Stereix_Session* outSession) {
    return NexVR_InitializeDX12(info, outSession);
}

static inline Stereix_Result Stereix_GetGraphicsRequirementsVulkan(Stereix_GraphicsRequirementsVulkan* requirements) {
    return NexVR_GetGraphicsRequirementsVulkan(requirements);
}

static inline Stereix_Result Stereix_InitializeVulkan(const Stereix_InitializeVulkanInfo* info, Stereix_Session* outSession) {
    return NexVR_InitializeVulkan(info, outSession);
}

static inline Stereix_Result Stereix_WaitFrame(Stereix_Session session, Stereix_FrameState* frameState) {
    return NexVR_WaitFrame(session, frameState);
}

static inline Stereix_Result Stereix_SubmitFrameDX11(Stereix_Session session, uint64_t frameToken, ID3D11Texture2D* renderTarget) {
    return NexVR_SubmitFrameDX11(session, frameToken, renderTarget);
}

static inline Stereix_Result Stereix_SubmitFrameWithDepthDX11(Stereix_Session session, uint64_t frameToken, ID3D11Texture2D* renderTarget, const Stereix_DepthInfoDX11* depthInfo) {
    return NexVR_SubmitFrameWithDepthDX11(session, frameToken, renderTarget, depthInfo);
}

static inline Stereix_Result Stereix_SubmitFrameDX12(Stereix_Session session, uint64_t frameToken, ID3D12Resource* renderTarget, D3D12_RESOURCE_STATES targetState) {
    return NexVR_SubmitFrameDX12(session, frameToken, renderTarget, targetState);
}

static inline Stereix_Result Stereix_SubmitFrameWithDepthDX12(Stereix_Session session, uint64_t frameToken, ID3D12Resource* renderTarget, D3D12_RESOURCE_STATES targetState, const Stereix_DepthInfoDX12* depthInfo) {
    return NexVR_SubmitFrameWithDepthDX12(session, frameToken, renderTarget, targetState, depthInfo);
}

static inline Stereix_Result Stereix_SubmitFrameVulkan(Stereix_Session session, uint64_t frameToken, VkImage image, VkImageLayout layout) {
    return NexVR_SubmitFrameVulkan(session, frameToken, image, layout);
}

static inline Stereix_Result Stereix_SubmitFrameWithDepthVulkan(Stereix_Session session, uint64_t frameToken, VkImage image, VkImageLayout layout, const Stereix_DepthInfoVulkan* depthInfo) {
    return NexVR_SubmitFrameWithDepthVulkan(session, frameToken, image, layout, depthInfo);
}

static inline Stereix_Result Stereix_PollEvents(Stereix_Session session) {
    return NexVR_PollEvents(session);
}

static inline Stereix_Result Stereix_GetTrackingState(Stereix_Session session, Stereix_TrackingOrigin origin, Stereix_TrackingState* outState) {
    return NexVR_GetTrackingState(session, origin, outState);
}

static inline Stereix_Result Stereix_GetControllerState(Stereix_Session session, Stereix_Hand hand, Stereix_ControllerState* outState) {
    return NexVR_GetControllerState(session, hand, outState);
}

static inline Stereix_Result Stereix_TriggerHaptic(Stereix_Session session, Stereix_Hand hand, const Stereix_HapticFeedback* haptic) {
    return NexVR_TriggerHaptic(session, hand, haptic);
}

static inline Stereix_Result Stereix_StopHaptic(Stereix_Session session, Stereix_Hand hand) {
    return NexVR_StopHaptic(session, hand);
}

static inline void Stereix_Shutdown(Stereix_Session session) {
    NexVR_Shutdown(session);
}

static inline const char* Stereix_GetLastErrorDetail(void) {
    return NexVR_GetLastErrorDetail();
}

#ifdef __cplusplus
}
#endif

#endif /* STEREIX_SDK_H */
