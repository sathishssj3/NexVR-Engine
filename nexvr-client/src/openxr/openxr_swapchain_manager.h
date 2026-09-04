#pragma once

#include "openxr/openxr_types.h"
#include "openxr/openxr_health_monitor.h"
#include <d3d11.h>
#include <d3d12.h>
#include <vector>
#include "core/graphics_types.h"

namespace vrinject {
namespace openxr {

class OpenXRSwapchainManager {
public:
    OpenXRSwapchainManager(OpenXRHealthMonitor* healthMonitor);
    ~OpenXRSwapchainManager();

    bool Initialize(XrSession session, int64_t format, uint32_t width, uint32_t height, GraphicsBackend backendAPI);
    void Shutdown();

    bool AcquireImages(uint32_t& outLeftIndex, uint32_t& outRightIndex);
    bool WaitImages();
    bool ReleaseImages();

    ID3D11Texture2D* GetLeftSwapchainImage(uint32_t index) const;
    ID3D11Texture2D* GetRightSwapchainImage(uint32_t index) const;

    ID3D12Resource* GetLeftSwapchainImageDX12(uint32_t index) const;
    ID3D12Resource* GetRightSwapchainImageDX12(uint32_t index) const;

    VkImage GetLeftSwapchainImageVulkan(uint32_t index) const;
    VkImage GetRightSwapchainImageVulkan(uint32_t index) const;

    XrSwapchain GetLeftSwapchain() const { return leftSwapchain_; }
    XrSwapchain GetRightSwapchain() const { return rightSwapchain_; }

private:
    bool CreateSwapchain(XrSession session, int64_t format, uint32_t width, uint32_t height, XrSwapchain& outSwapchain, GraphicsBackend backendAPI, bool isLeft);

    OpenXRHealthMonitor* healthMonitor_ = nullptr;
    
    XrSwapchain leftSwapchain_ = XR_NULL_HANDLE;
    XrSwapchain rightSwapchain_ = XR_NULL_HANDLE;

    std::vector<XrSwapchainImageD3D11KHR> leftImagesD3D11_;
    std::vector<XrSwapchainImageD3D11KHR> rightImagesD3D11_;
    std::vector<XrSwapchainImageD3D12KHR> leftImagesD3D12_;
    std::vector<XrSwapchainImageD3D12KHR> rightImagesD3D12_;
    std::vector<XrSwapchainImageVulkanKHR> leftImagesVulkan_;
    std::vector<XrSwapchainImageVulkanKHR> rightImagesVulkan_;
};

} // namespace openxr
} // namespace vrinject
