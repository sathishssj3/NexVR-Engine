#include "openxr/openxr_swapchain_manager.h"
#include <iostream>

namespace vrinject {
namespace openxr {

OpenXRSwapchainManager::OpenXRSwapchainManager(OpenXRHealthMonitor* healthMonitor)
    : healthMonitor_(healthMonitor) {}

OpenXRSwapchainManager::~OpenXRSwapchainManager() {
    Shutdown();
}

bool OpenXRSwapchainManager::Initialize(XrSession session, int64_t format, uint32_t width, uint32_t height, GraphicsBackend backendAPI) {
    if (!CreateSwapchain(session, format, width, height, leftSwapchain_, backendAPI, true)) {
        return false;
    }
    if (!CreateSwapchain(session, format, width, height, rightSwapchain_, backendAPI, false)) {
        return false;
    }
    return true;
}

bool OpenXRSwapchainManager::CreateSwapchain(XrSession session, int64_t format, uint32_t width, uint32_t height, XrSwapchain& outSwapchain, GraphicsBackend backendAPI, bool isLeft) {
    XrSwapchainCreateInfo createInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    createInfo.arraySize = 1;
    createInfo.format = format;
    createInfo.width = width;
    createInfo.height = height;
    createInfo.mipCount = 1;
    createInfo.faceCount = 1;
    createInfo.sampleCount = 1;
    createInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;

    XrResult res = xrCreateSwapchain(session, &createInfo, &outSwapchain);
    if (XR_FAILED(res)) {
        std::cerr << "Failed to create OpenXR swapchain.\n";
        return false;
    }

    uint32_t imageCount = 0;
    xrEnumerateSwapchainImages(outSwapchain, 0, &imageCount, nullptr);
    
    if (backendAPI == GraphicsBackend::DX12) {
        std::vector<XrSwapchainImageD3D12KHR>& outImages = isLeft ? leftImagesD3D12_ : rightImagesD3D12_;
        outImages.clear();
        outImages.resize(imageCount);
        for (uint32_t i = 0; i < imageCount; ++i) {
            outImages[i] = {XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR};
            outImages[i].next = nullptr;
        }
        res = xrEnumerateSwapchainImages(outSwapchain, imageCount, &imageCount, reinterpret_cast<XrSwapchainImageBaseHeader*>(outImages.data()));
    } else if (backendAPI == GraphicsBackend::Vulkan) {
        std::vector<XrSwapchainImageVulkanKHR>& outImages = isLeft ? leftImagesVulkan_ : rightImagesVulkan_;
        outImages.clear();
        outImages.resize(imageCount);
        for (uint32_t i = 0; i < imageCount; ++i) {
            outImages[i] = {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR};
            outImages[i].next = nullptr;
        }
        res = xrEnumerateSwapchainImages(outSwapchain, imageCount, &imageCount, reinterpret_cast<XrSwapchainImageBaseHeader*>(outImages.data()));
    } else {
        std::vector<XrSwapchainImageD3D11KHR>& outImages = isLeft ? leftImagesD3D11_ : rightImagesD3D11_;
        outImages.clear();
        outImages.resize(imageCount);
        for (uint32_t i = 0; i < imageCount; ++i) {
            outImages[i] = {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR};
            outImages[i].next = nullptr;
        }
        res = xrEnumerateSwapchainImages(outSwapchain, imageCount, &imageCount, reinterpret_cast<XrSwapchainImageBaseHeader*>(outImages.data()));
    }
    
    if (XR_FAILED(res)) {
        std::cerr << "Failed to enumerate OpenXR swapchain images.\n";
        return false;
    }

    return true;
}

void OpenXRSwapchainManager::Shutdown() {
    if (leftSwapchain_ != XR_NULL_HANDLE) {
        xrDestroySwapchain(leftSwapchain_);
        leftSwapchain_ = XR_NULL_HANDLE;
    }
    if (rightSwapchain_ != XR_NULL_HANDLE) {
        xrDestroySwapchain(rightSwapchain_);
        rightSwapchain_ = XR_NULL_HANDLE;
    }
    leftImagesD3D11_.clear();
    rightImagesD3D11_.clear();
    leftImagesD3D12_.clear();
    rightImagesD3D12_.clear();
    leftImagesVulkan_.clear();
    rightImagesVulkan_.clear();
}

bool OpenXRSwapchainManager::AcquireImages(uint32_t& outLeftIndex, uint32_t& outRightIndex) {
    XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    
    XrResult resL = xrAcquireSwapchainImage(leftSwapchain_, &acquireInfo, &outLeftIndex);
    if (XR_FAILED(resL)) {
        if (healthMonitor_) healthMonitor_->RecordAcquireTimeout();
        return false;
    }

    XrResult resR = xrAcquireSwapchainImage(rightSwapchain_, &acquireInfo, &outRightIndex);
    if (XR_FAILED(resR)) {
        if (healthMonitor_) healthMonitor_->RecordAcquireTimeout();
        return false;
    }
    return true;
}

bool OpenXRSwapchainManager::WaitImages() {
    XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    // 50ms bounded timeout prevents freezing/hanging the game loop if runtime stalls
    waitInfo.timeout = 50000000; // 50ms in nanoseconds

    XrResult resL = xrWaitSwapchainImage(leftSwapchain_, &waitInfo);
    if (XR_FAILED(resL)) return false;

    XrResult resR = xrWaitSwapchainImage(rightSwapchain_, &waitInfo);
    if (XR_FAILED(resR)) return false;

    return true;
}

bool OpenXRSwapchainManager::ReleaseImages() {
    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};

    XrResult resL = xrReleaseSwapchainImage(leftSwapchain_, &releaseInfo);
    if (XR_FAILED(resL)) return false;

    XrResult resR = xrReleaseSwapchainImage(rightSwapchain_, &releaseInfo);
    if (XR_FAILED(resR)) return false;

    return true;
}

ID3D11Texture2D* OpenXRSwapchainManager::GetLeftSwapchainImage(uint32_t index) const {
    if (index >= leftImagesD3D11_.size()) return nullptr;
    return leftImagesD3D11_[index].texture;
}

ID3D11Texture2D* OpenXRSwapchainManager::GetRightSwapchainImage(uint32_t index) const {
    if (index >= rightImagesD3D11_.size()) return nullptr;
    return rightImagesD3D11_[index].texture;
}

ID3D12Resource* OpenXRSwapchainManager::GetLeftSwapchainImageDX12(uint32_t index) const {
    if (index >= leftImagesD3D12_.size()) return nullptr;
    return leftImagesD3D12_[index].texture;
}

ID3D12Resource* OpenXRSwapchainManager::GetRightSwapchainImageDX12(uint32_t index) const {
    if (index >= rightImagesD3D12_.size()) return nullptr;
    return rightImagesD3D12_[index].texture;
}

VkImage OpenXRSwapchainManager::GetLeftSwapchainImageVulkan(uint32_t index) const {
    if (index >= leftImagesVulkan_.size()) return VK_NULL_HANDLE;
    return leftImagesVulkan_[index].image;
}

VkImage OpenXRSwapchainManager::GetRightSwapchainImageVulkan(uint32_t index) const {
    if (index >= rightImagesVulkan_.size()) return VK_NULL_HANDLE;
    return rightImagesVulkan_[index].image;
}

} // namespace openxr
} // namespace vrinject
