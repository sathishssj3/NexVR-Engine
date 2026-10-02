/*===========================================================================
  stereix_sdk.h — Stereix Engine B2B SDK, Official C ABI
  Copyright (c) 2026 Mesmeran Lab. All rights reserved.
  ---------------------------------------------------------------------------
  DirectX 11 & DirectX 12 real-time VR injection & spatial rendering runtime
  for enterprise game studios and simulation developers.
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

#define STEREIX_SUCCESS                        NEXVR_SUCCESS
#define STEREIX_SESSION_EXITING                NEXVR_SESSION_EXITING
#define STEREIX_FRAME_SKIPPED                  NEXVR_FRAME_SKIPPED

#define STEREIX_ERROR_RUNTIME_UNAVAILABLE      NEXVR_ERROR_RUNTIME_UNAVAILABLE
#define STEREIX_ERROR_NO_HEADSET               NEXVR_ERROR_NO_HEADSET
#define STEREIX_ERROR_REQUIREMENTS_NOT_QUERIED  NEXVR_ERROR_REQUIREMENTS_NOT_QUERIED
#define STEREIX_ERROR_WRONG_ADAPTER            NEXVR_ERROR_WRONG_ADAPTER
#define STEREIX_ERROR_DIMENSION_MISMATCH       NEXVR_ERROR_DIMENSION_MISMATCH
#define STEREIX_ERROR_INVALID_FORMAT           NEXVR_ERROR_INVALID_FORMAT
#define STEREIX_ERROR_NOT_IMMEDIATE_CONTEXT    NEXVR_ERROR_NOT_IMMEDIATE_CONTEXT
#define STEREIX_ERROR_INVALID_COMMAND_QUEUE    NEXVR_ERROR_INVALID_COMMAND_QUEUE
#define STEREIX_ERROR_COLOR_SPACE_UNSPECIFIED  NEXVR_ERROR_COLOR_SPACE_UNSPECIFIED
#define STEREIX_ERROR_MULTITHREAD_UNAVAILABLE  NEXVR_ERROR_MULTITHREAD_UNAVAILABLE
#define STEREIX_ERROR_INVALID_FRAME_TOKEN      NEXVR_ERROR_INVALID_FRAME_TOKEN
#define STEREIX_ERROR_SWAPCHAIN_TIMEOUT        NEXVR_ERROR_SWAPCHAIN_TIMEOUT
#define STEREIX_ERROR_INVALID_ARGUMENT         NEXVR_ERROR_INVALID_ARGUMENT
#define STEREIX_ERROR_INVALID_SESSION          NEXVR_ERROR_INVALID_SESSION
#define STEREIX_ERROR_RUNTIME_FAILURE          NEXVR_ERROR_RUNTIME_FAILURE

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

/* --- Handles & Structures ------------------------------------------------- */
typedef NexVR_Session Stereix_Session;

typedef NexVR_GraphicsRequirements        Stereix_GraphicsRequirements;
typedef NexVR_GraphicsRequirements        Stereix_GraphicsRequirementsDX12;

typedef NexVR_InitializeDX11Info          Stereix_InitializeDX11Info;
typedef NexVR_InitializeDX12Info          Stereix_InitializeDX12Info;

typedef NexVR_DepthInfoDX11               Stereix_DepthInfoDX11;
typedef NexVR_DepthInfoDX12               Stereix_DepthInfoDX12;

typedef NexVR_Pose                        Stereix_Pose;
typedef NexVR_View                        Stereix_View;
typedef NexVR_FrameState                  Stereix_FrameState;
typedef NexVR_ControllerState             Stereix_ControllerState;
typedef NexVR_HapticFeedback              Stereix_HapticFeedback;

/* --- Core Function Declarations (Exported from stereix_sdk.dll / nexvr_sdk.dll) */

STEREIX_API Stereix_Result STEREIX_CALL
Stereix_GetGraphicsRequirements(Stereix_GraphicsRequirements* requirements);

STEREIX_API Stereix_Result STEREIX_CALL
Stereix_GetGraphicsRequirementsDX12(Stereix_GraphicsRequirements* requirements);

STEREIX_API Stereix_Result STEREIX_CALL
Stereix_InitializeDX11(const Stereix_InitializeDX11Info* info, Stereix_Session* outSession);

STEREIX_API Stereix_Result STEREIX_CALL
Stereix_InitializeDX12(const Stereix_InitializeDX12Info* info, Stereix_Session* outSession);

STEREIX_API Stereix_Result STEREIX_CALL
Stereix_WaitFrame(Stereix_Session session, Stereix_FrameState* frameState);

STEREIX_API Stereix_Result STEREIX_CALL
Stereix_SubmitFrameDX11(Stereix_Session session, uint64_t frameToken, ID3D11Texture2D* renderTarget);

STEREIX_API Stereix_Result STEREIX_CALL
Stereix_SubmitFrameWithDepthDX11(Stereix_Session session, uint64_t frameToken,
                                 ID3D11Texture2D* renderTarget, const Stereix_DepthInfoDX11* depthInfo);

STEREIX_API uint32_t STEREIX_CALL
Stereix_SupportsDepthSubmission(Stereix_Session session);

STEREIX_API Stereix_Result STEREIX_CALL
Stereix_SubmitFrameDX12(Stereix_Session session, uint64_t frameToken,
                        ID3D12Resource* renderTarget, D3D12_RESOURCE_STATES targetState);

STEREIX_API Stereix_Result STEREIX_CALL
Stereix_SubmitFrameWithDepthDX12(Stereix_Session session, uint64_t frameToken,
                                 ID3D12Resource* renderTarget, D3D12_RESOURCE_STATES targetState,
                                 const Stereix_DepthInfoDX12* depthInfo);

STEREIX_API Stereix_Result STEREIX_CALL
Stereix_SyncInput(Stereix_Session session);

STEREIX_API Stereix_Result STEREIX_CALL
Stereix_GetControllerState(Stereix_Session session, Stereix_Hand hand, Stereix_ControllerState* outState);

STEREIX_API Stereix_Result STEREIX_CALL
Stereix_TriggerHaptic(Stereix_Session session, Stereix_Hand hand, const Stereix_HapticFeedback* haptic);

STEREIX_API Stereix_Result STEREIX_CALL
Stereix_StopHaptic(Stereix_Session session, Stereix_Hand hand);

STEREIX_API void STEREIX_CALL
Stereix_Shutdown(Stereix_Session session);

STEREIX_API const char* STEREIX_CALL
Stereix_GetLastErrorDetail(void);

#ifdef __cplusplus
}
#endif

#endif /* STEREIX_SDK_H */
