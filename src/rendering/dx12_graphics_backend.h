#pragma once

#include "igraphics_backend.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <mutex>
#include <memory>
#include "dx12_stereo_resource_manager.h"
#include "dx12_pipeline_state_cache.h"
#include "dx12_resource_state_tracker.h"
#include "dx12_fence_manager.h"
#include "dx12_stereo_renderer.h"

namespace vrinject {

class DX12GraphicsBackend : public IGraphicsBackend {
public:
    DX12GraphicsBackend();
    ~DX12GraphicsBackend() override;

    bool Initialize(void* nativeDevice, void* nativeContext) override;
    void Shutdown() override;

    void SetState(StereoRendererState state) override;
    StereoRendererState GetState() const override;

    bool Healthy() const override;

    CameraSnapshot GetCamera() override;
    DepthSnapshot GetDepth() override;

    void RenderStereo(
        const CameraSnapshot& camSnapshot, 
        const DepthSnapshot& depthSnapshot, 
        const StereoParams& params
    ) override;
    
    void* GetLeftEyeTexture() override;
    void* GetRightEyeTexture() override;
    
    GraphicsBackend GetAPI() const override { return GraphicsBackend::DX12; }
    
    // OpenXR Interop
    void SetOpenXRSwapchainImages(ID3D12Resource* left, ID3D12Resource* right);

    // Test support
    DX12FenceManager* GetFenceManager() { return m_fenceManager.get(); }

private:
    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
    
    // Subsystems
    std::unique_ptr<DX12StereoResourceManager> m_resourceManager;
    std::unique_ptr<DX12PipelineStateCache> m_psoCache;
    std::unique_ptr<DX12ResourceStateTracker> m_stateTracker;
    std::unique_ptr<DX12FenceManager> m_fenceManager;
    std::unique_ptr<DX12StereoRenderer> m_renderer;

    StereoRendererState m_state = StereoRendererState::UNINITIALIZED;
    mutable std::mutex m_mutex;

    ID3D12Resource* m_oxrLeftDest = nullptr;
    ID3D12Resource* m_oxrRightDest = nullptr;
};

} // namespace vrinject
