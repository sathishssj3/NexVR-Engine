#include "rendering/dx12/dx12_stereo_resource_manager.h"
#include "rendering/dx12/dx12_lifecycle_manager.h"
#include <iostream>

namespace vrinject {

DX12StereoResourceManager::~DX12StereoResourceManager() {
    Shutdown();
}

bool DX12StereoResourceManager::Initialize(ID3D12Device* device, DX12FenceManager* fenceManager, DX12PipelineStateCache* psoCache) {
    if (!device || !fenceManager || !psoCache) return false;

    m_device = device;
    m_fenceManager = fenceManager;
    m_psoCache = psoCache;

    m_descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    if (!CreateDescriptorHeaps()) return false;
    if (!CreateConstantBuffer()) return false;

    return true;
}

void DX12StereoResourceManager::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // We get the last signaled fence value and defer our releases
    uint64_t fenceValue = m_fenceManager ? m_fenceManager->GetLastSignaledValue() : 0;
    
    if (m_fenceManager) {
        if (m_leftEyeTexture) m_fenceManager->DeferRelease(m_leftEyeTexture, fenceValue);
        if (m_rightEyeTexture) m_fenceManager->DeferRelease(m_rightEyeTexture, fenceValue);
        if (m_constantBuffer) {
            if (m_mappedConstantBuffer) {
                m_constantBuffer->Unmap(0, nullptr);
                m_mappedConstantBuffer = nullptr;
            }
            m_fenceManager->DeferRelease(m_constantBuffer, fenceValue);
        }
        if (m_cbvSrvUavHeap) m_fenceManager->DeferRelease(m_cbvSrvUavHeap, fenceValue);
    }
    
    m_leftEyeTexture.Reset();
    m_rightEyeTexture.Reset();
    m_constantBuffer.Reset();
    m_cbvSrvUavHeap.Reset();
}

bool DX12StereoResourceManager::HandleResize(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return false;
    if (width == m_currentWidth && height == m_currentHeight) return true;

    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_currentWidth = width;
    m_currentHeight = height;

    // Defer old textures
    uint64_t fenceValue = m_fenceManager->GetLastSignaledValue();
    if (m_leftEyeTexture) m_fenceManager->DeferRelease(m_leftEyeTexture, fenceValue);
    if (m_rightEyeTexture) m_fenceManager->DeferRelease(m_rightEyeTexture, fenceValue);
    m_leftEyeTexture.Reset();
    m_rightEyeTexture.Reset();

    return CreateEyeTextures(width, height);
}

bool DX12StereoResourceManager::CreateEyeTextures(uint32_t width, uint32_t height) {
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    if (FAILED(m_device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &desc, 
            D3D12_RESOURCE_STATE_COMMON, nullptr, 
            IID_PPV_ARGS(&m_leftEyeTexture)))) {
        return false;
    }
    m_leftEyeTexture->SetName(L"NexVR_LeftEyeTexture");

    if (FAILED(m_device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &desc, 
            D3D12_RESOURCE_STATE_COMMON, nullptr, 
            IID_PPV_ARGS(&m_rightEyeTexture)))) {
        return false;
    }
    m_rightEyeTexture->SetName(L"NexVR_RightEyeTexture");

    // Recreate UAVs
    D3D12_CPU_DESCRIPTOR_HANDLE uavHandle = m_cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
    uavHandle.ptr += 3 * m_descriptorSize; // Skip CBV(1) + SRV(2)

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = desc.Format;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    
    m_device->CreateUnorderedAccessView(m_leftEyeTexture.Get(), nullptr, &uavDesc, uavHandle);
    uavHandle.ptr += m_descriptorSize;
    m_device->CreateUnorderedAccessView(m_rightEyeTexture.Get(), nullptr, &uavDesc, uavHandle);

    return true;
}

bool DX12StereoResourceManager::CreateDescriptorHeaps() {
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = 5; // 1 CBV + 2 SRV (Cam, Depth) + 2 UAV (Left, Right)
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    if (FAILED(m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_cbvSrvUavHeap)))) {
        return false;
    }
    m_cbvSrvUavHeap->SetName(L"NexVR_StereoDescriptorHeap");
    
    return true;
}

bool DX12StereoResourceManager::CreateConstantBuffer() {
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = 256; // Aligned to 256 bytes
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    if (FAILED(m_device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &desc, 
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, 
            IID_PPV_ARGS(&m_constantBuffer)))) {
        return false;
    }

    if (FAILED(m_constantBuffer->Map(0, nullptr, &m_mappedConstantBuffer))) {
        return false;
    }

    // Create CBV
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = m_constantBuffer->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = 256;

    D3D12_CPU_DESCRIPTOR_HANDLE cbvHandle = m_cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
    m_device->CreateConstantBufferView(&cbvDesc, cbvHandle);

    return true;
}

void DX12StereoResourceManager::BindDescriptorHeaps(ID3D12GraphicsCommandList* commandList) {
    if (m_cbvSrvUavHeap) {
        ID3D12DescriptorHeap* heaps[] = { m_cbvSrvUavHeap.Get() };
        commandList->SetDescriptorHeaps(1, heaps);
    }
}

void DX12StereoResourceManager::UpdateFrameResources(ID3D12Device* device, const CameraSnapshot& cam, const DepthSnapshot& depth) {
    // HandleResize has its own internal locking — call it before acquiring m_mutex
    // to avoid recursive lock deadlock
    if (cam.IsValid() && cam.resourceIdentity.width > 0 && cam.resourceIdentity.height > 0) {
        if (cam.resourceIdentity.width != m_currentWidth || cam.resourceIdentity.height != m_currentHeight) {
            HandleResize(cam.resourceIdentity.width, cam.resourceIdentity.height);
        }
    } else {
        UINT scWidth = Dx12LifecycleManager::Get().GetWidth();
        UINT scHeight = Dx12LifecycleManager::Get().GetHeight();
        if (scWidth > 0 && scHeight > 0 && (scWidth != m_currentWidth || scHeight != m_currentHeight)) {
            HandleResize(scWidth, scHeight);
        }
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Update constant buffer data
    if (m_mappedConstantBuffer) {
        // Here we'd map reprojection matrices. 
        // memcpy(m_mappedConstantBuffer, &stereoData, sizeof(StereoData));
    }

    UpdateViews(cam, depth);
}

void DX12StereoResourceManager::UpdateViews(const CameraSnapshot& cam, const DepthSnapshot& depth) {
    // Offset to SRV section
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
    srvHandle.ptr += m_descriptorSize; // Skip CBV

    if (cam.IsValid() && cam.resourceIdentity.nativeHandle) {
        ID3D12Resource* camRes = reinterpret_cast<ID3D12Resource*>(cam.resourceIdentity.nativeHandle);
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // Assuming based on typical output
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        m_device->CreateShaderResourceView(camRes, &srvDesc, srvHandle);
    }
    
    srvHandle.ptr += m_descriptorSize;

    if (depth.IsValid() && depth.identity.nativeHandle) {
        ID3D12Resource* depthRes = reinterpret_cast<ID3D12Resource*>(depth.identity.nativeHandle);
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        // Map depth format to its SRV-compatible typeless alias
        switch (depth.identity.format) {
            case DXGI_FORMAT_D32_FLOAT:
                srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
                break;
            case DXGI_FORMAT_D24_UNORM_S8_UINT:
                srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
                break;
            case DXGI_FORMAT_D16_UNORM:
                srvDesc.Format = DXGI_FORMAT_R16_UNORM;
                break;
            default:
                srvDesc.Format = DXGI_FORMAT_R32_FLOAT; // Fallback
                break;
        }
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        m_device->CreateShaderResourceView(depthRes, &srvDesc, srvHandle);
    }
}

D3D12_GPU_DESCRIPTOR_HANDLE DX12StereoResourceManager::GetCbvGpuHandle() const {
    if (!m_cbvSrvUavHeap) return {0};
    return m_cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
}

D3D12_GPU_DESCRIPTOR_HANDLE DX12StereoResourceManager::GetSrvGpuHandle() const {
    if (!m_cbvSrvUavHeap) return {0};
    auto handle = m_cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += m_descriptorSize;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE DX12StereoResourceManager::GetUavGpuHandle() const {
    if (!m_cbvSrvUavHeap) return {0};
    auto handle = m_cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += 3 * m_descriptorSize;
    return handle;
}

} // namespace vrinject
