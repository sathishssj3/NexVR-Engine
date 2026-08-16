#pragma once
#include "graphics_types.h"
#include <cstdint>

#include "../rendering/openxr/openxr.h"

namespace vrinject {

enum class RenderState {
    UNINITIALIZED,
    DISCOVERING,
    INITIALIZING,
    RUNNING,
    RESIZE_PENDING,
    REBUILDING_RESOURCES,
    DEVICE_REMOVED,
    RECOVERING,
    REINITIALIZING,
    RECOVERY_FAILED,
    DEGRADED,
    SHUTTING_DOWN,
    STOPPED
};

struct RenderFrameSnapshot {
    GraphicsBackend backend = GraphicsBackend::Unknown;
    GraphicsResourceEpoch epoch;
    RenderState state = RenderState::UNINITIALIZED;
    
    // OpenXR Head Pose
    XrPosef leftPose = {{0, 0, 0, 1}, {0, 0, 0}};
    XrFovf leftFov = {-0.8f, 0.8f, 0.8f, -0.8f};
    XrPosef rightPose = {{0, 0, 0, 1}, {0, 0, 0}};
    XrFovf rightFov = {-0.8f, 0.8f, 0.8f, -0.8f};

    // Abstract backend contexts
    void* nativeDevice = nullptr;       // ID3D11Device*, ID3D12Device*, or VkDevice
    void* nativeContext = nullptr;      // ID3D11DeviceContext*, ID3D12CommandQueue*, or VkQueue
    void* nativeInstance = nullptr;     // VkInstance (Vulkan only)
    void* nativePhysicalDevice = nullptr; // VkPhysicalDevice (Vulkan only)
    void* nativeSwapchain = nullptr;    // IDXGISwapChain* or VkSwapchainKHR
    void* nativeSurface = nullptr;      // VkSurfaceKHR (Vulkan only)
    
    // Swapchain metadata
    void* backBuffer = nullptr;         // ID3D11Texture2D*, ID3D12Resource*, or VkImage
    int64_t format = 0;                 // DXGI_FORMAT or VkFormat
    uint32_t width = 0;
    uint32_t height = 0;
    bool fullscreen = false;

    // Vulkan specific loader info
    uint32_t apiVersion = 0;
    uint32_t driverVersion = 0;
    
    bool IsValid() const {
        bool baseValid = nativeDevice != nullptr && nativeContext != nullptr && nativeSwapchain != nullptr && width > 0 && height > 0;
        if (backend == GraphicsBackend::Vulkan) {
            return baseValid && nativeSurface != nullptr;
        }
        return baseValid;
    }

    // The swapchain colour image expressed as a GraphicsResourceIdentity. The stereo
    // pass reprojects this texture (see Dx12StereoRenderer::RenderStereo), so camera
    // snapshots must carry it -- CameraLockManager is backend-agnostic and cannot
    // discover it, exactly as DepthLockManager is handed its resource context.
    GraphicsResourceIdentity BackBufferIdentity() const {
        GraphicsResourceIdentity identity;
        identity.backend = backend;
        identity.nativeHandle = backBuffer;
        identity.width = width;
        identity.height = height;
        identity.format = format;
        identity.epoch = epoch;
        return identity;
    }
};

} // namespace vrinject
