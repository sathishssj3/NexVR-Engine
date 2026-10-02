#pragma once

// Must include windows.h before OpenXR on Windows
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <unknwn.h>
#include <d3d11.h>
#include <d3d12.h>
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#define XR_USE_GRAPHICS_API_D3D12
#define XR_USE_GRAPHICS_API_VULKAN
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

namespace vrinject {
namespace openxr {

enum class RuntimeState {
    UNINITIALIZED,
    INSTANCE_CREATED,
    SYSTEM_SELECTED,
    SESSION_CREATED,
    SESSION_READY,
    RUNNING,
    FOCUSED,
    VISIBLE,
    LOSS_PENDING,
    STOPPING,
    STOPPED,
    FAILED
};

enum class SubmitterState {
    WAIT_FRAME,
    BEGIN_FRAME,
    ACQUIRE_IMAGES,
    WAIT_IMAGES,
    COPY,
    RELEASE,
    END_FRAME
};

} // namespace openxr
} // namespace vrinject
