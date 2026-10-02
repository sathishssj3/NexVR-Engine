/*===========================================================================
  stereix_sdk.h — Stereix Engine B2B SDK, C ABI
  Copyright (c) 2026 Mesmeran Lab. All rights reserved.
  ---------------------------------------------------------------------------
  Stereix Engine ABI bridge header providing official Stereix-branded
  types, structures, and entry points with 100% binary-compatible mapping
  to runtime core symbols.
===========================================================================*/

#ifndef STEREIX_SDK_H
#define STEREIX_SDK_H

#include "nexvr_sdk.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Type Aliases */
typedef NexVR_Result                Stereix_Result;
typedef NexVR_Config                Stereix_Config;
typedef NexVR_ViewMatrix            Stereix_ViewMatrix;
typedef NexVR_FrameDesc             Stereix_FrameDesc;
typedef NexVR_ControllerState       Stereix_ControllerState;
typedef NexVR_TrackingOrigin        Stereix_TrackingOrigin;
typedef NexVR_ColorSpace            Stereix_ColorSpace;
typedef NexVR_GraphicsApi           Stereix_GraphicsApi;

/* Common Status Constants */
#define STEREIX_SUCCESS             NEXVR_SUCCESS
#define STEREIX_ERROR_GENERIC       NEXVR_ERROR_GENERIC
#define STEREIX_ERROR_INVALID_PARAM NEXVR_ERROR_INVALID_PARAM
#define STEREIX_ERROR_NOT_SUPPORTED NEXVR_ERROR_NOT_SUPPORTED

#ifdef __cplusplus
}
#endif

#endif /* STEREIX_SDK_H */
