#pragma once
#include <cstdint>
#include <compare>

namespace vrinject {

enum class GraphicsBackend {
    Unknown,
    DX11,
    DX12,
    Vulkan
};

struct GraphicsResourceEpoch {
    uint64_t deviceGeneration = 0;
    uint64_t swapchainGeneration = 0;
    uint64_t resourceGeneration = 0;
    uint64_t queueGeneration = 0;
    uint64_t surfaceGeneration = 0;

    auto operator<=>(const GraphicsResourceEpoch&) const = default;
};

struct GraphicsResourceIdentity {
    GraphicsBackend backend = GraphicsBackend::Unknown;
    void* nativeHandle = nullptr;  // e.g. ID3D11Texture2D*, ID3D12Resource*, or VkImage
    uint32_t width = 0;
    uint32_t height = 0;
    int64_t format = 0;            // DXGI_FORMAT or VkFormat depending on backend
    uint32_t sampleCount = 1;
    uint32_t arraySize = 1;
    uint32_t mipLevels = 1;
    GraphicsResourceEpoch epoch;

    bool IsValid() const {
        return nativeHandle != nullptr && width > 0 && height > 0;
    }

    bool operator==(const GraphicsResourceIdentity& other) const {
        return backend == other.backend &&
               nativeHandle == other.nativeHandle &&
               width == other.width &&
               height == other.height &&
               format == other.format &&
               sampleCount == other.sampleCount &&
               arraySize == other.arraySize &&
               mipLevels == other.mipLevels &&
               epoch == other.epoch;
    }
};

} // namespace vrinject
