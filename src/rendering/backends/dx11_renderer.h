#pragma once
#include "../irenderer.h"
#include <d3d11.h>

class DX11Renderer : public IRenderer {
public:
    bool Initialize(void* nativeDevice, void* nativeContext) override;
    void Shutdown() override;

    TextureHandle CreateTexture(uint32_t width, uint32_t height,
                                uint32_t format, bool uav) override;
    void DestroyTexture(TextureHandle& handle) override;

    ShaderHandle LoadComputeShader(const uint8_t* bytecode,
                                   size_t bytecodeSize) override;
    void DestroyShader(ShaderHandle& handle) override;

    void DispatchCompute(ShaderHandle shader,
                         TextureHandle input, TextureHandle output,
                         uint32_t groupsX, uint32_t groupsY) override;

    void CopyToSwapchain(TextureHandle source,
                         void* swapchainTexture) override;

    GraphicsAPI GetAPI() const override { return GraphicsAPI::DX11; }

    // Expose raw device for OpenXR binding struct construction
    ID3D11Device*        GetDevice()  const { return m_device;  }
    ID3D11DeviceContext* GetContext() const { return m_context; }

private:
    ID3D11Device*        m_device  = nullptr;
    ID3D11DeviceContext* m_context = nullptr;
};
