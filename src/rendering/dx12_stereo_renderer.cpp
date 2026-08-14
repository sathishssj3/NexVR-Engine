#include "dx12_stereo_renderer.h"
#include <iostream>
#include <cassert>

namespace vrinject {

DX12StereoRenderer::~DX12StereoRenderer() {
    Shutdown();
}

bool DX12StereoRenderer::Initialize(ID3D12Device* device) {
    if (!device) return false;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[i])))) {
            return false;
        }
    }

    if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&m_commandList)))) {
        return false;
    }

    // Command list starts in recording state, close it immediately since we reset it per frame
    m_commandList->Close();

    return true;
}

void DX12StereoRenderer::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_commandList.Reset();
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_commandAllocators[i].Reset();
    }
}

bool DX12StereoRenderer::RenderStereo(
    ID3D12CommandQueue* gameQueue,
    DX12StereoResourceManager* resourceManager,
    DX12PipelineStateCache* psoCache,
    DX12ResourceStateTracker* stateTracker,
    DX12FenceManager* fenceManager,
    const CameraSnapshot& cam,
    const DepthSnapshot& depth,
    ID3D12Resource* oxrLeftDest,
    ID3D12Resource* oxrRightDest) 
{
    if (!gameQueue || !resourceManager || !psoCache || !stateTracker || !fenceManager) return false;

    // Synchronization Contract (Single Queue Assertion)
    // The dispatch MUST submit to the game's queue to guarantee FIFO ordering
    if (gameQueue != Dx12LifecycleManager::Get().GetMainQueue()) {
        std::cerr << "CRITICAL ERROR: StereoRenderer dispatch queue does not match LifecycleManager MainQueue." << std::endl;
        assert(false && "Single-Queue synchronization contract violated!");
        m_isDegraded = true;
    }

    if (m_isDegraded) {
        return false; // Skip stereo rendering if degraded
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);

    // Frame Sequence
    uint32_t frameIndex = (cam.frame > 0 ? cam.frame : 0) % MAX_FRAMES_IN_FLIGHT;

    // Ensure the GPU has finished with this allocator before resetting it
    if (m_frameFences[frameIndex] != 0) {
        fenceManager->WaitForFenceValue(m_frameFences[frameIndex]);
    }
    
    m_commandAllocators[frameIndex]->Reset();
    m_commandList->Reset(m_commandAllocators[frameIndex].Get(), nullptr);

    bool isStereoValid = (cam.IsValid() && depth.IsValid() && cam.resourceIdentity.nativeHandle && depth.identity.nativeHandle);

    if (!isStereoValid) {
        // Fallback: 2D Passthrough / Virtual Cinema mode (e.g. In Menus/UI)
        ID3D12Resource* backBuffer = Dx12LifecycleManager::Get().GetBackBuffer();
        if (backBuffer && (oxrLeftDest || oxrRightDest)) {
            stateTracker->ForceResourceState(backBuffer, D3D12_RESOURCE_STATE_PRESENT);
            stateTracker->TransitionResource(backBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE);

            if (oxrLeftDest) {
                stateTracker->ForceResourceState(oxrLeftDest, D3D12_RESOURCE_STATE_COMMON);
                stateTracker->TransitionResource(oxrLeftDest, D3D12_RESOURCE_STATE_COPY_DEST);
            }
            if (oxrRightDest) {
                stateTracker->ForceResourceState(oxrRightDest, D3D12_RESOURCE_STATE_COMMON);
                stateTracker->TransitionResource(oxrRightDest, D3D12_RESOURCE_STATE_COPY_DEST);
            }
            stateTracker->FlushBarriers(m_commandList.Get());

            if (oxrLeftDest) {
                m_commandList->CopyResource(oxrLeftDest, backBuffer);
            }
            if (oxrRightDest) {
                m_commandList->CopyResource(oxrRightDest, backBuffer);
            }

            stateTracker->TransitionResource(backBuffer, D3D12_RESOURCE_STATE_PRESENT);
            if (oxrLeftDest) {
                stateTracker->TransitionResource(oxrLeftDest, D3D12_RESOURCE_STATE_COMMON);
            }
            if (oxrRightDest) {
                stateTracker->TransitionResource(oxrRightDest, D3D12_RESOURCE_STATE_COMMON);
            }
            stateTracker->FlushBarriers(m_commandList.Get());

            m_commandList->Close();
            ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
            gameQueue->ExecuteCommandLists(1, ppCommandLists);
            m_frameFences[frameIndex] = fenceManager->Signal(gameQueue);
            return true;
        }

        m_commandList->Close();
        return true;
    }
    
    // Transition Resources (Inputs)
    ID3D12Resource* leftEye = resourceManager->GetLeftEyeTexture();
    ID3D12Resource* rightEye = resourceManager->GetRightEyeTexture();
    
    // Eye textures are created lazily on first HandleResize.
    // If they don't exist yet, skip this frame cleanly.
    if (!leftEye || !rightEye) {
        m_commandList->Close();
        return true; // Not an error — just not ready yet
    }
    
    ID3D12Resource* camTex = reinterpret_cast<ID3D12Resource*>(cam.resourceIdentity.nativeHandle);
    ID3D12Resource* depthTex = reinterpret_cast<ID3D12Resource*>(depth.identity.nativeHandle);
    
    // Check PSO availability once, before recording any commands
    ID3D12Device* device = nullptr;
    gameQueue->GetDevice(IID_PPV_ARGS(&device));
    bool hasPso = false;
    ID3D12RootSignature* rootSig = nullptr;
    ID3D12PipelineState* pso = nullptr;
    if (device) {
        rootSig = psoCache->GetStereoRootSignature(device);
        pso = psoCache->GetStereoPSO(device);
        hasPso = (rootSig != nullptr && pso != nullptr);
        device->Release();
    }
    
    if (!hasPso) {
        // No PSO yet — submit empty command list to keep fence cadence valid
        m_commandList->Close();
        ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
        gameQueue->ExecuteCommandLists(1, ppCommandLists);
        m_frameFences[frameIndex] = fenceManager->Signal(gameQueue);
        return true;
    }
    
    // Full render path: barriers → bind → dispatch → barriers → execute
    stateTracker->TransitionResource(camTex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    stateTracker->TransitionResource(depthTex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    stateTracker->TransitionResource(leftEye, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    stateTracker->TransitionResource(rightEye, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    stateTracker->FlushBarriers(m_commandList.Get());

    // Bind Descriptor Heaps
    resourceManager->BindDescriptorHeaps(m_commandList.Get());

    // Bind Root Signature & Pipeline State
    m_commandList->SetComputeRootSignature(rootSig);
    m_commandList->SetPipelineState(pso);

    // Bind Constants & Resources
    m_commandList->SetComputeRootDescriptorTable(0, resourceManager->GetCbvGpuHandle());
    m_commandList->SetComputeRootDescriptorTable(1, resourceManager->GetSrvGpuHandle());
    m_commandList->SetComputeRootDescriptorTable(2, resourceManager->GetUavGpuHandle());

    // Dispatch Compute (8x8 thread groups)
    UINT dispatchX = (cam.resourceIdentity.width + 7) / 8;
    UINT dispatchY = (cam.resourceIdentity.height + 7) / 8;
    m_commandList->Dispatch(dispatchX, dispatchY, 1);

    // UAV Barrier to ensure compute writes are visible before copy to swapchain
    D3D12_RESOURCE_BARRIER uavBarriers[2] = {};
    uavBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarriers[0].UAV.pResource = leftEye;
    uavBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarriers[1].UAV.pResource = rightEye;
    m_commandList->ResourceBarrier(2, uavBarriers);

    // Transition Outputs for OpenXR Copy Source
    stateTracker->TransitionResource(leftEye, D3D12_RESOURCE_STATE_COPY_SOURCE);
    stateTracker->TransitionResource(rightEye, D3D12_RESOURCE_STATE_COPY_SOURCE);
    
    // Transition OpenXR swapchain images to COPY_DEST
    // Note: OpenXR expects the app to leave images in a presentable state.
    // D3D12_RESOURCE_STATE_COMMON works as a neutral baseline that OpenXR can transition from.
    if (oxrLeftDest) {
        stateTracker->ForceResourceState(oxrLeftDest, D3D12_RESOURCE_STATE_COMMON); // Assume OpenXR gave it to us in COMMON
        stateTracker->TransitionResource(oxrLeftDest, D3D12_RESOURCE_STATE_COPY_DEST);
    }
    if (oxrRightDest) {
        stateTracker->ForceResourceState(oxrRightDest, D3D12_RESOURCE_STATE_COMMON);
        stateTracker->TransitionResource(oxrRightDest, D3D12_RESOURCE_STATE_COPY_DEST);
    }
    stateTracker->FlushBarriers(m_commandList.Get());

    // Copy to OpenXR Swapchain
    if (oxrLeftDest) {
        m_commandList->CopyResource(oxrLeftDest, leftEye);
    }
    if (oxrRightDest) {
        m_commandList->CopyResource(oxrRightDest, rightEye);
    }

    // Transition OpenXR swapchain images back to COMMON for OpenXR presentation
    if (oxrLeftDest) {
        stateTracker->TransitionResource(oxrLeftDest, D3D12_RESOURCE_STATE_COMMON);
    }
    if (oxrRightDest) {
        stateTracker->TransitionResource(oxrRightDest, D3D12_RESOURCE_STATE_COMMON);
    }
    stateTracker->FlushBarriers(m_commandList.Get());

    // Execute Command List
    m_commandList->Close();
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    gameQueue->ExecuteCommandLists(1, ppCommandLists);

    // Signal Fence (GPU Lifetime tracking)
    m_frameFences[frameIndex] = fenceManager->Signal(gameQueue);
    
    return true;
}

} // namespace vrinject
