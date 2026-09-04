#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <mutex>
#include "rendering/dx12/dx12_fence_manager.h"
#include "rendering/dx12/dx12_pipeline_state_cache.h"
#include "core/graphics_types.h"
#include "heuristics/camera_snapshot.h"
#include "heuristics/depth_snapshot.h"

#include "core/stereo_types.h"

namespace vrinject {

// Descriptor handle structure for easy tracking
struct DX12DescriptorAllocation {
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
    uint32_t heapIndex;
};

struct DX12StereoShaderConstants {
    Matrix4x4 inverseViewProj;
    Matrix4x4 leftViewProj;
    Matrix4x4 rightViewProj;
    Vector3 originalEyePos;
    float pad0;
    Vector3 leftEyePos;
    float pad1;
    Vector3 rightEyePos;
    float pad2;
    uint32_t width;
    uint32_t height;
    uint32_t shouldAttemptStereo;
    float padding;
};

class DX12StereoResourceManager {
public:
    DX12StereoResourceManager() = default;
    ~DX12StereoResourceManager();

    bool Initialize(ID3D12Device* device, DX12FenceManager* fenceManager, DX12PipelineStateCache* psoCache);
    void Shutdown();

    // Recreate resources if the window/resolution changes
    bool HandleResize(uint32_t width, uint32_t height);

    // Binds the necessary heaps to the command list
    void BindDescriptorHeaps(ID3D12GraphicsCommandList* commandList);

    // Updates CBV and shader resource views for the current frame
    void UpdateFrameResources(
        ID3D12Device* device, 
        const EyeView& leftEye,
        const EyeView& rightEye,
        const CameraSnapshot& cam, 
        const DepthSnapshot& depth,
        bool shouldAttemptStereo);

    // Resources accessors
    ID3D12Resource* GetLeftEyeTexture() const { return m_leftEyeTexture.Get(); }
    ID3D12Resource* GetRightEyeTexture() const { return m_rightEyeTexture.Get(); }
    uint32_t GetWidth() const { return m_currentWidth; }
    uint32_t GetHeight() const { return m_currentHeight; }
    
    // Heaps
    ID3D12DescriptorHeap* GetCbvSrvUavHeap() const { return m_cbvSrvUavHeap.Get(); }

    // Constant buffer location
    D3D12_GPU_DESCRIPTOR_HANDLE GetCbvGpuHandle() const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuHandle() const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetUavGpuHandle() const;

private:
    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    DX12FenceManager* m_fenceManager = nullptr;
    DX12PipelineStateCache* m_psoCache = nullptr;

    uint32_t m_currentWidth = 0;
    uint32_t m_currentHeight = 0;

    // Rendering resources
    Microsoft::WRL::ComPtr<ID3D12Resource> m_leftEyeTexture;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_rightEyeTexture;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_constantBuffer;
    
    // Memory mapped CBV
    void* m_mappedConstantBuffer = nullptr;

    // Descriptor Heaps
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_cbvSrvUavHeap;
    UINT m_descriptorSize = 0;

    mutable std::mutex m_mutex;

    bool CreateEyeTextures(uint32_t width, uint32_t height);
    bool CreateDescriptorHeaps();
    bool CreateConstantBuffer();
    
    void UpdateViews(const CameraSnapshot& cam, const DepthSnapshot& depth);
};

} // namespace vrinject
