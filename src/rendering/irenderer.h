#pragma once
#include <cstdint>

// Opaque handles — backend fills these in, caller treats them as black boxes
struct TextureHandle {
    void* nativePtr  = nullptr;   // ID3D11Texture2D*, ID3D12Resource*, VkImage
    void* nativeView = nullptr;   // SRV / descriptor handle / VkImageView
    void* nativeUAV  = nullptr;   // UAV (for DX11/DX12)
    uint32_t width   = 0;
    uint32_t height  = 0;
};

struct ShaderHandle {
    void* pipelineState  = nullptr;   // PSO or VkPipeline
    void* rootSignature  = nullptr;   // DX12 root sig / VkPipelineLayout
    void* shaderBytecode = nullptr;   // for reuse/debug
};

enum class GraphicsAPI {
    DX11,
    DX12,
    VULKAN,
    UNKNOWN
};

class IRenderer {
public:
    virtual ~IRenderer() = default;

    // Lifecycle
    virtual bool Initialize(void* nativeDevice, void* nativeContext) = 0;
    virtual void Shutdown() = 0;

    // Resources
    virtual TextureHandle CreateTexture(
        uint32_t width, uint32_t height,
        uint32_t format,        // DXGI_FORMAT or VkFormat cast to uint32_t
        bool uav) = 0;
    virtual void DestroyTexture(TextureHandle& handle) = 0;

    // Shaders — accept pre-compiled bytecode byte arrays
    virtual ShaderHandle LoadComputeShader(
        const uint8_t* bytecode,
        size_t bytecodeSize) = 0;
    virtual void DestroyShader(ShaderHandle& handle) = 0;

    // Dispatch
    virtual void DispatchCompute(
        ShaderHandle shader,
        const TextureHandle* inputs, uint32_t numInputs,
        const TextureHandle* outputs, uint32_t numOutputs,
        const void* constantsData, size_t constantsSize,
        uint32_t groupsX, uint32_t groupsY) = 0;

    virtual void ClearUAVUint(TextureHandle texture, const uint32_t values[4]) = 0;
    virtual void CopyTexture(TextureHandle dst, TextureHandle src) = 0;

    // Swapchain copy — MUST be in the abstraction to keep OpenXRManager API-agnostic
    virtual void CopyToSwapchain(
        TextureHandle source,
        void* swapchainTexture) = 0;   // XrSwapchainImageD3D11KHR.texture etc.

    virtual void CopyToSwapchainRect(TextureHandle source,
                         void* swapchainTexture,
                         uint32_t srcX, uint32_t srcY,
                         uint32_t width, uint32_t height) {
        // Default implementation falls back to full copy
        CopyToSwapchain(source, swapchainTexture);
    }

    virtual GraphicsAPI GetAPI() const = 0;
};
