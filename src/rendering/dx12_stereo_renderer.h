#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <mutex>
#include "../core/camera_snapshot.h"
#include "../core/depth_snapshot.h"
#include "dx12_fence_manager.h"
#include "dx12_pipeline_state_cache.h"
#include "dx12_resource_state_tracker.h"
#include "dx12_stereo_resource_manager.h"
#include "../core/dx12_lifecycle_manager.h"

namespace vrinject {

class DX12StereoRenderer {
public:
    DX12StereoRenderer() = default;
    ~DX12StereoRenderer();

    bool Initialize(ID3D12Device* device);
    void Shutdown();

    // Renders the stereo reprojection. Returns true on success, false if degraded/skipped.
    // Enforces the single-queue constraint.
    bool RenderStereo(
        ID3D12CommandQueue* gameQueue,
        DX12StereoResourceManager* resourceManager,
        DX12PipelineStateCache* psoCache,
        DX12ResourceStateTracker* stateTracker,
        DX12FenceManager* fenceManager,
        const CameraSnapshot& cam,
        const DepthSnapshot& depth,
        ID3D12Resource* oxrLeftDest = nullptr,
        ID3D12Resource* oxrRightDest = nullptr);

private:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocators[MAX_FRAMES_IN_FLIGHT];
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;
    uint64_t m_frameFences[MAX_FRAMES_IN_FLIGHT] = {0, 0, 0};
    
    mutable std::mutex m_mutex;
    
    bool m_isDegraded = false;
};

} // namespace vrinject
