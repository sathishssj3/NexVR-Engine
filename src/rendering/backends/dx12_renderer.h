#pragma once
#include "../irenderer.h"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

class DX12Renderer : public IRenderer {
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

    GraphicsAPI GetAPI() const override { return GraphicsAPI::DX12; }

    ID3D12Device*       GetDevice()       const { return m_device; }
    ID3D12CommandQueue* GetCommandQueue() const { return m_commandQueue; }

private:
    ID3D12Device*              m_device       = nullptr;
    ID3D12CommandQueue*        m_commandQueue = nullptr;
    ID3D12CommandAllocator*    m_cmdAlloc[2]  = {nullptr, nullptr};
    uint32_t                   m_frameIndex   = 0;
    ID3D12GraphicsCommandList* m_cmdList      = nullptr;

    // Simple descriptor heaps for this prototype
    ID3D12DescriptorHeap* m_srvUavHeap = nullptr;
    UINT m_srvUavDescriptorSize = 0;
    UINT m_descriptorCount = 0;

    void AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE& cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE& gpuHandle);
};
