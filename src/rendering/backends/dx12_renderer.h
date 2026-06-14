#pragma once
#include "../irenderer.h"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <atomic>
#include <mutex>
#include <d3d11on12.h>
#include "../stereo_pipeline.h"

class DX12Renderer : public IRenderer {
public:
    bool Initialize(void* nativeDevice, void* nativeContext) override;
    void Shutdown() override;

    void SetVRFormat(int64_t format) { m_vrFormat = format; }
    void SkipVRFrame();

    TextureHandle CreateTexture(uint32_t width, uint32_t height,
                                uint32_t format, bool uav) override;
    void DestroyTexture(TextureHandle& handle) override;
    void Flush();

    ShaderHandle LoadComputeShader(const uint8_t* bytecode,
                                   size_t bytecodeSize) override;
    void DestroyShader(ShaderHandle& handle) override;

    void LoadTonemapShader();
    TextureHandle CreateIntermediateTexture(uint32_t width, uint32_t height, int64_t overrideFormat = 0, bool allowUav = true);

    void DispatchCompute(ShaderHandle shader, TextureHandle input, TextureHandle output, uint32_t groupsX, uint32_t groupsY) override {}

    void ExecuteTonemapToIntermediate(TextureHandle source);
    void CopyToSwapchainVR(void* swapchainTexture, const vrinject::StereoParams* params = nullptr);

    void CopyToSwapchain(TextureHandle source,
                         void* swapchainTexture) override {}

    void SwapIndices() { 
        std::lock_guard<std::mutex> lock(m_indexMutex);
        m_readIndex.store(m_writeIndex.load()); 
        m_writeIndex.store((m_writeIndex.load() + 1) % 2); 
    }

    GraphicsAPI GetAPI() const override { return GraphicsAPI::DX12; }

    ID3D12Device*       GetDevice()       const { return m_device; }
    ID3D12CommandQueue* GetVRCommandQueue() const { return m_vrCommandQueue; }

    void UpdateGameCommandQueue(void* newQueue);

    void SetVRResolutionAndFormat(uint32_t width, uint32_t height, int64_t format);
    
    uint32_t m_vrWidth = 0;
    uint32_t m_vrHeight = 0;
    int64_t m_vrFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

    // Expose double-buffer slot indices for latency compensation capture
    int GetWriteIndex() const { return m_writeIndex.load(std::memory_order_acquire); }
    int GetReadIndex()  const { return m_readIndex.load(std::memory_order_acquire); }

private:
    ID3D12Device*              m_device       = nullptr;
    ID3D12CommandQueue* m_gameCommandQueue = nullptr;
    ID3D12CommandQueue* m_vrCommandQueue = nullptr;

    // D3D11On12 Interop
    ID3D11Device* m_d3d11Device = nullptr;
    ID3D11DeviceContext* m_d3d11Context = nullptr;
    ID3D11On12Device* m_d3d11On12Device = nullptr;
    vrinject::StereoPipeline m_stereoPipeline;

    // Concurrency / Synchronization for worker queue
    ID3D12Fence* m_syncFence = nullptr;
    HANDLE m_syncFenceEvent = nullptr;
    uint64_t m_currentFenceValue = 0;
    uint64_t m_fenceValues[2] = {0, 0};
    uint64_t m_allocatorFenceValues[2] = {0, 0};

    // VR Queue synchronization
    ID3D12Fence* m_vrFence = nullptr;
    HANDLE m_vrFenceEvent = nullptr;
    uint64_t m_vrFenceValue = 0;
    uint64_t m_vrReadFenceValues[2] = {0, 0};

    // Worker queue resources (tonemap)
    ID3D12CommandAllocator* m_cmdAlloc[2] = {nullptr, nullptr};
    uint32_t                   m_frameIndex   = 0;
    ID3D12GraphicsCommandList* m_cmdList      = nullptr;

    // VR queue resources
    ID3D12CommandAllocator*    m_vrCmdAlloc[2] = {nullptr, nullptr};
    uint32_t                   m_vrFrameIndex  = 0;
    ID3D12GraphicsCommandList* m_vrCmdList     = nullptr;

    std::mutex m_indexMutex;
    std::mutex m_vrQueueMutex;

    // Simple descriptor heaps for this prototype
    ID3D12DescriptorHeap* m_srvUavHeap = nullptr;
    UINT m_srvUavDescriptorSize = 0;
    UINT m_descriptorCount = 0;
    ShaderHandle m_tonemapShader = {};
    
    TextureHandle m_intermediateTextures[2] = {};
    TextureHandle m_rawIntermediateTextures[2] = {};
    DXGI_FORMAT m_rawTextureFormats[2] = { DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN }; // Bug 4: track format
    std::atomic<int> m_writeIndex = 0;
    std::atomic<int> m_readIndex = 1;

    void AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE& cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE& gpuHandle);
};
