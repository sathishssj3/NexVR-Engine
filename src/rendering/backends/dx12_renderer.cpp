#include "dx12_renderer.h"
#include <vector>
#include <string>
#include <windows.h>
#include <algorithm>
#include <d3dcompiler.h>



bool DX12Renderer::Initialize(void* nativeDevice, void* nativeContext) {
    if (!nativeDevice || !nativeContext) return false;
    
    m_device = static_cast<ID3D12Device*>(nativeDevice);
    m_gameCommandQueue = static_cast<ID3D12CommandQueue*>(nativeContext);
    
    m_device->AddRef();
    m_gameCommandQueue->AddRef();

    // Create VR command queue
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (FAILED(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_vrCommandQueue)))) {
        return false;
    }

    // Initialize D3D11On12
    UINT d3d11DeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    IUnknown* commandQueues[] = { m_vrCommandQueue };
    if (SUCCEEDED(D3D11On12CreateDevice(m_device, d3d11DeviceFlags, nullptr, 0, commandQueues, 1, 0, &m_d3d11Device, &m_d3d11Context, nullptr))) {
        m_d3d11Device->QueryInterface(IID_PPV_ARGS(&m_d3d11On12Device));
    } else {
        return false;
    }

    // Worker fence — used to synchronize our worker queue's command allocator reuse
    if (FAILED(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_syncFence)))) return false;
    if (FAILED(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_vrFence)))) return false;
    m_syncFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    m_vrFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    // Command allocators and list for our worker queue
    for (int i = 0; i < 2; ++i) {
        if (FAILED(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_cmdAlloc[i])))) return false;
        if (FAILED(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_vrCmdAlloc[i])))) return false;
    }
    
    if (FAILED(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_cmdAlloc[0], nullptr, IID_PPV_ARGS(&m_cmdList)))) return false;
    if (FAILED(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_vrCmdAlloc[0], nullptr, IID_PPV_ARGS(&m_vrCmdList)))) return false;
    m_cmdList->Close();
    m_vrCmdList->Close();

    // Create descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = 1024;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_srvUavHeap)))) {
        return false;
    }
    m_srvUavDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    LoadTonemapShader();

    return true;
}

void DX12Renderer::UpdateGameCommandQueue(void* newQueue) {
    if (!newQueue) return;
    ID3D12CommandQueue* queue = static_cast<ID3D12CommandQueue*>(newQueue);
    if (m_gameCommandQueue != queue) {
        // Wait for the old queue to completely drain before replacing it
        if (m_gameCommandQueue && m_syncFence) {
            Flush();
        }
        if (m_gameCommandQueue) {
            m_gameCommandQueue->Release();
        }
        m_gameCommandQueue = queue;
        m_gameCommandQueue->AddRef();
    }
}

void DX12Renderer::Shutdown() {
    Flush();
    if (m_cmdList) { m_cmdList->Release(); m_cmdList = nullptr; }
    if (m_vrCmdList) { m_vrCmdList->Release(); m_vrCmdList = nullptr; }
    for (int i = 0; i < 2; ++i) {
        if (m_cmdAlloc[i]) { m_cmdAlloc[i]->Release(); m_cmdAlloc[i] = nullptr; }
        if (m_vrCmdAlloc[i]) { m_vrCmdAlloc[i]->Release(); m_vrCmdAlloc[i] = nullptr; }
    }
    if (m_srvUavHeap) { m_srvUavHeap->Release(); m_srvUavHeap = nullptr; }
    DestroyShader(m_tonemapShader);
    for (int i = 0; i < 2; ++i) {
        if (m_intermediateTextures[i].nativePtr) DestroyTexture(m_intermediateTextures[i]);
        if (m_rawIntermediateTextures[i].nativePtr) DestroyTexture(m_rawIntermediateTextures[i]);
    }
    if (m_syncFence) { m_syncFence->Release(); m_syncFence = nullptr; }
    if (m_vrFence) { m_vrFence->Release(); m_vrFence = nullptr; }
    if (m_syncFenceEvent) { CloseHandle(m_syncFenceEvent); m_syncFenceEvent = nullptr; }
    if (m_vrFenceEvent) { CloseHandle(m_vrFenceEvent); m_vrFenceEvent = nullptr; }
    if (m_gameCommandQueue) { m_gameCommandQueue->Release(); m_gameCommandQueue = nullptr; }
    if (m_vrCommandQueue) { m_vrCommandQueue->Release(); m_vrCommandQueue = nullptr; }
    if (m_d3d11On12Device) { m_d3d11On12Device->Release(); m_d3d11On12Device = nullptr; }
    if (m_d3d11Context) { m_d3d11Context->Release(); m_d3d11Context = nullptr; }
    if (m_d3d11Device) { m_d3d11Device->Release(); m_d3d11Device = nullptr; }
    m_stereoPipeline.Shutdown();

    if (m_device) { m_device->Release(); m_device = nullptr; }
}

void DX12Renderer::SetVRResolutionAndFormat(uint32_t width, uint32_t height, int64_t format) {
    m_vrWidth = width;
    m_vrHeight = height;
    m_vrFormat = format;
    
    if (m_d3d11Device && width > 0 && height > 0) {
        char modulePath[MAX_PATH];
        GetModuleFileNameA(reinterpret_cast<HMODULE>(GetModuleHandleA("vrinject.dll")), modulePath, MAX_PATH);
        std::string moduleDir = modulePath;
        size_t pos = moduleDir.find_last_of("\\/");
        if (pos != std::string::npos) moduleDir = moduleDir.substr(0, pos);
        
        m_stereoPipeline.Initialize(m_d3d11Device, width, height, moduleDir);
    }
}

void DX12Renderer::AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE& cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE& gpuHandle) {
    if (m_descriptorCount >= 1024) m_descriptorCount = 0;
    cpuHandle = m_srvUavHeap->GetCPUDescriptorHandleForHeapStart();
    cpuHandle.ptr += m_descriptorCount * m_srvUavDescriptorSize;
    gpuHandle = m_srvUavHeap->GetGPUDescriptorHandleForHeapStart();
    gpuHandle.ptr += m_descriptorCount * m_srvUavDescriptorSize;
    m_descriptorCount++;
}

TextureHandle DX12Renderer::CreateTexture(uint32_t width, uint32_t height, uint32_t format, bool uav) {
    return CreateIntermediateTexture(width, height, format, uav);
}

TextureHandle DX12Renderer::CreateIntermediateTexture(uint32_t width, uint32_t height, int64_t overrideFormat, bool allowUav) {
    TextureHandle handle = {};
    if (!m_device) return handle;

    DXGI_FORMAT actualFormat = overrideFormat != 0 ? static_cast<DXGI_FORMAT>(overrideFormat) : static_cast<DXGI_FORMAT>(m_vrFormat);
    
    DXGI_FORMAT resourceFormat = actualFormat;
    DXGI_FORMAT uavFormat = actualFormat;

    // We cannot create a UAV for an SRGB texture. We must create it as TYPELESS
    // and create a UNORM UAV, while the SRV or Copy source will remain compatible.
    if (allowUav) {
        if (actualFormat == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) {
            resourceFormat = DXGI_FORMAT_R8G8B8A8_TYPELESS;
            uavFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        } else if (actualFormat == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) {
            resourceFormat = DXGI_FORMAT_B8G8R8A8_TYPELESS;
            uavFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
        }
    }

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = resourceFormat;
    desc.SampleDesc.Count = 1;
    desc.Flags = allowUav ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    ID3D12Resource* tex = nullptr;
    if (SUCCEEDED(m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&tex)))) {
        handle.nativePtr = tex;
        handle.width = width;
        handle.height = height;

        if (allowUav) {
            D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
            D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
            AllocateDescriptor(cpuHandle, gpuHandle);
            
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.Format = uavFormat;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            uavDesc.Texture2D.MipSlice = 0;
            
            m_device->CreateUnorderedAccessView(tex, nullptr, &uavDesc, cpuHandle);
            handle.nativeView = reinterpret_cast<void*>(gpuHandle.ptr);
        }
    }
    return handle;
}

void DX12Renderer::DestroyTexture(TextureHandle& handle) {
    if (handle.nativePtr) {
        static_cast<ID3D12Resource*>(handle.nativePtr)->Release();
        handle.nativePtr = nullptr;
        handle.nativeView = nullptr; // Also clear the cached view
    }
}

void DX12Renderer::Flush() {
    if (!m_device) return;
    
    if (m_syncFence) {
        uint64_t maxSync = m_fenceValues[0] > m_fenceValues[1] ? m_fenceValues[0] : m_fenceValues[1];
        if (maxSync > 0 && m_syncFence->GetCompletedValue() < maxSync) {
            m_syncFence->SetEventOnCompletion(maxSync, m_syncFenceEvent);
            WaitForSingleObject(m_syncFenceEvent, INFINITE);
        }
    }
    
    if (m_vrFence) {
        uint64_t maxVr = m_vrReadFenceValues[0] > m_vrReadFenceValues[1] ? m_vrReadFenceValues[0] : m_vrReadFenceValues[1];
        if (maxVr > 0 && m_vrFence->GetCompletedValue() < maxVr) {
            m_vrFence->SetEventOnCompletion(maxVr, m_vrFenceEvent);
            WaitForSingleObject(m_vrFenceEvent, INFINITE);
        }
    }
}



ShaderHandle DX12Renderer::LoadComputeShader(const uint8_t* bytecode, size_t bytecodeSize) {
    ShaderHandle handle = {};
    if (!m_device || !bytecode || bytecodeSize == 0) return handle;

    D3D12_DESCRIPTOR_RANGE ranges[2] = {};
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[0].NumDescriptors = 1;
    ranges[0].BaseShaderRegister = 0;
    ranges[0].RegisterSpace = 0;
    ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[1].NumDescriptors = 1;
    ranges[1].BaseShaderRegister = 0;
    ranges[1].RegisterSpace = 0;
    ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParams[2] = {};
    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[0].DescriptorTable.pDescriptorRanges = &ranges[0];
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[1].DescriptorTable.pDescriptorRanges = &ranges[1];
    rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
    rootDesc.NumParameters = 2;
    rootDesc.pParameters = rootParams;
    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ID3DBlob* signatureBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    if (FAILED(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob))) {
        if (errorBlob) errorBlob->Release();
        return handle;
    }

    ID3D12RootSignature* rootSignature = nullptr;
    if (FAILED(m_device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)))) {
        signatureBlob->Release();
        return handle;
    }
    
    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.CS.pShaderBytecode = bytecode;
    psoDesc.CS.BytecodeLength = bytecodeSize;
    psoDesc.pRootSignature = rootSignature;

    ID3D12PipelineState* pso = nullptr;
    if (SUCCEEDED(m_device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pso)))) {
        handle.pipelineState = pso;
        handle.rootSignature = rootSignature;
    }
    signatureBlob->Release();
    return handle;
}

void DX12Renderer::DestroyShader(ShaderHandle& handle) {
    if (handle.pipelineState) static_cast<ID3D12PipelineState*>(handle.pipelineState)->Release();
    if (handle.rootSignature) static_cast<ID3D12RootSignature*>(handle.rootSignature)->Release();
    handle.pipelineState = nullptr; handle.rootSignature = nullptr;
}



#include "../../../build/bin/shaders/tonemap_cs_dx12.h"
void DX12Renderer::LoadTonemapShader() {
    m_tonemapShader = LoadComputeShader(g_tonemap_DX12, sizeof(g_tonemap_DX12));
}

void DX12Renderer::ExecuteTonemapToIntermediate(TextureHandle source) {
    if (!m_device || !source.nativePtr) return;

    // Wait for our worker queue's command allocator to be idle before reusing
    m_frameIndex = (m_frameIndex + 1) % 2;
    uint64_t fenceToWaitFor = m_allocatorFenceValues[m_frameIndex];
    if (fenceToWaitFor > 0 && m_syncFence->GetCompletedValue() < fenceToWaitFor) {
        m_syncFence->SetEventOnCompletion(fenceToWaitFor, m_syncFenceEvent);
        WaitForSingleObject(m_syncFenceEvent, INFINITE);
    }
    m_cmdAlloc[m_frameIndex]->Reset();
    
    int writeIdx;
    uint64_t vrFenceToWaitFor;
    {
        std::lock_guard<std::mutex> lock(m_indexMutex);
        writeIdx = m_writeIndex.load();
        vrFenceToWaitFor = m_vrReadFenceValues[writeIdx];
    }
    
    uint32_t targetW = (m_vrWidth > 0) ? m_vrWidth : source.width;
    uint32_t targetH = (m_vrHeight > 0) ? m_vrHeight : source.height;

    // Use < instead of != so VR target texture is only grown, never recreated for DRS
    if (!m_intermediateTextures[writeIdx].nativePtr || m_intermediateTextures[writeIdx].width < targetW || m_intermediateTextures[writeIdx].height < targetH) {
        Flush();
        if (m_intermediateTextures[writeIdx].nativePtr) DestroyTexture(m_intermediateTextures[writeIdx]);
        m_intermediateTextures[writeIdx] = CreateIntermediateTexture(targetW, targetH, 0, true);
    }
    
    if (m_tonemapShader.pipelineState && m_intermediateTextures[writeIdx].nativePtr && m_tonemapShader.rootSignature) {
        m_cmdList->Reset(m_cmdAlloc[m_frameIndex], static_cast<ID3D12PipelineState*>(m_tonemapShader.pipelineState));
        m_cmdList->SetComputeRootSignature(static_cast<ID3D12RootSignature*>(m_tonemapShader.rootSignature));

        ID3D12DescriptorHeap* heaps[] = { m_srvUavHeap };
        m_cmdList->SetDescriptorHeaps(1, heaps);

        ID3D12Resource* srcRes = static_cast<ID3D12Resource*>(source.nativePtr);
        DXGI_FORMAT srvFmt = srcRes->GetDesc().Format;

        bool formatChanged = m_rawTextureFormats[writeIdx] != srvFmt;
        
        if (!m_rawIntermediateTextures[writeIdx].nativePtr || 
            m_rawIntermediateTextures[writeIdx].width < source.width || 
            m_rawIntermediateTextures[writeIdx].height < source.height ||
            formatChanged) {
            Flush();
            if (m_rawIntermediateTextures[writeIdx].nativePtr) DestroyTexture(m_rawIntermediateTextures[writeIdx]);
            if (m_intermediateTextures[writeIdx].nativePtr) DestroyTexture(m_intermediateTextures[writeIdx]);
            
            // Raw intermediate MUST match the exact format of the source (so CopyTextureRegion works)
            m_rawIntermediateTextures[writeIdx] = CreateIntermediateTexture(source.width, source.height, srvFmt, false);
            // Final intermediate uses the standard pipeline format (OpenXR's selected format)
            m_intermediateTextures[writeIdx] = CreateIntermediateTexture(source.width, source.height, 0, true);
            m_rawTextureFormats[writeIdx] = srvFmt;
        }
        ID3D12Resource* rawRes = static_cast<ID3D12Resource*>(m_rawIntermediateTextures[writeIdx].nativePtr);
        ID3D12Resource* midRes = static_cast<ID3D12Resource*>(m_intermediateTextures[writeIdx].nativePtr);

        if (!rawRes || !midRes) return;

        // Transition the source backbuffer from PRESENT instead of COMMON.
        // We submit on the game's queue, and the game has already transitioned it to PRESENT 
        // because it is about to call Present(). We must return it to PRESENT when done.
        D3D12_RESOURCE_BARRIER barriers[3] = {};
        barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[0].Transition.pResource = srcRes;
        barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        
        barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[1].Transition.pResource = rawRes;
        barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
        barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

        m_cmdList->ResourceBarrier(2, barriers);

        // Use a box to copy only the source region (handles DRS smaller-than-allocated textures)
        D3D12_BOX srcBox = {};
        srcBox.left = 0;
        srcBox.top = 0;
        srcBox.front = 0;
        srcBox.right = source.width;
        srcBox.bottom = source.height;
        srcBox.back = 1;

        D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
        dstLoc.pResource = rawRes;
        dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLoc.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
        srcLoc.pResource = srcRes;
        srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        srcLoc.SubresourceIndex = 0;

        m_cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, &srcBox);

        // Transition back: source to PRESENT (for DXGI), raw to SRV, mid to UAV
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        
        barriers[2].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[2].Transition.pResource = midRes;
        barriers[2].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barriers[2].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
        barriers[2].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        
        m_cmdList->ResourceBarrier(3, barriers);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = srvFmt;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        if (!m_rawIntermediateTextures[writeIdx].nativeView) {
            D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
            D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
            AllocateDescriptor(cpuHandle, gpuHandle);
            m_device->CreateShaderResourceView(rawRes, &srvDesc, cpuHandle);
            m_rawIntermediateTextures[writeIdx].nativeView = reinterpret_cast<void*>(gpuHandle.ptr);
        }
        m_cmdList->SetComputeRootDescriptorTable(0, *reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(&m_rawIntermediateTextures[writeIdx].nativeView));
        
        if (m_intermediateTextures[writeIdx].nativeView) {
            m_cmdList->SetComputeRootDescriptorTable(1, *reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(&m_intermediateTextures[writeIdx].nativeView));
        }

        m_cmdList->Dispatch( (source.width + 7)/8, (source.height + 7)/8, 1 );

        barriers[0].Transition.pResource = rawRes;
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
        barriers[1].Transition.pResource = midRes;
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
        m_cmdList->ResourceBarrier(2, barriers);
        m_cmdList->Close();
        
        // Submit on the GAME's queue. D3D12 ExecuteCommandLists IS thread-safe!
        // This ensures our copy is ordered perfectly before the DXGI presentation engine kicks in.
        
        // Wait for the VR queue to finish reading from this write slot
        if (vrFenceToWaitFor > 0) {
            m_gameCommandQueue->Wait(m_vrFence, vrFenceToWaitFor);
        }
        
        ID3D12CommandList* lists[] = { m_cmdList };
        m_gameCommandQueue->ExecuteCommandLists(1, lists);

        m_currentFenceValue++;
        m_gameCommandQueue->Signal(m_syncFence, m_currentFenceValue);
        m_fenceValues[writeIdx] = m_currentFenceValue;
        m_allocatorFenceValues[m_frameIndex] = m_currentFenceValue;
        
        // SwapIndices is called here, which is correct timing —
        // it happens after we've submitted the GPU work and signalled the fence.
        // The VR thread will read the new readIndex and wait on the fence value.
        SwapIndices();
    }
}

void DX12Renderer::SkipVRFrame() {
    int readIdx;
    uint64_t newFenceValue;
    {
        std::lock_guard<std::mutex> lock(m_indexMutex);
        readIdx = m_readIndex.load();
        m_vrFenceValue++;
        newFenceValue = m_vrFenceValue;
    }
    
    if (!m_device || !m_vrCommandQueue) return;

    if (m_vrFence->GetCompletedValue() < newFenceValue - 1) {
        m_vrFence->SetEventOnCompletion(newFenceValue - 1, m_vrFenceEvent);
        WaitForSingleObject(m_vrFenceEvent, INFINITE);
    }

    {
        std::lock_guard<std::mutex> vrLock(m_vrQueueMutex);
        m_vrCommandQueue->Signal(m_vrFence, newFenceValue);
    }
    
    {
        std::lock_guard<std::mutex> lock(m_indexMutex);
        m_vrReadFenceValues[readIdx] = newFenceValue;
    }
}

void DX12Renderer::CopyToSwapchainVR(void* swapchainTexture, const vrinject::StereoParams* params) {
    int readIdx;
    uint64_t newFenceValue;
    {
        std::lock_guard<std::mutex> lock(m_indexMutex);
        readIdx = m_readIndex.load();
        m_vrFenceValue++;
        newFenceValue = m_vrFenceValue;
    }
    if (!m_device || !m_intermediateTextures[readIdx].nativePtr || !swapchainTexture) {
        if (m_vrCommandQueue) {
            std::lock_guard<std::mutex> vrLock(m_vrQueueMutex);
            m_vrCommandQueue->Signal(m_vrFence, newFenceValue);
        }
        std::lock_guard<std::mutex> lock(m_indexMutex);
        m_vrReadFenceValues[readIdx] = newFenceValue;
        return;
    }

    if (m_vrFence->GetCompletedValue() < newFenceValue - 1) {
        m_vrFence->SetEventOnCompletion(newFenceValue - 1, m_vrFenceEvent);
        WaitForSingleObject(m_vrFenceEvent, INFINITE);
    }

    std::lock_guard<std::mutex> vrLock(m_vrQueueMutex);

    m_vrFrameIndex = (m_vrFrameIndex + 1) % 2;
    m_vrCmdAlloc[m_vrFrameIndex]->Reset();
    m_vrCmdList->Reset(m_vrCmdAlloc[m_vrFrameIndex], nullptr);
    
    // Wait for our worker queue to finish writing this slot
    uint64_t requiredFenceValue = m_fenceValues[readIdx];
    if (requiredFenceValue > 0) {
        m_vrCommandQueue->Wait(m_syncFence, requiredFenceValue);
    }
    
    ID3D12Resource* midRes = static_cast<ID3D12Resource*>(m_intermediateTextures[readIdx].nativePtr);
    ID3D12Resource* dstRes = static_cast<ID3D12Resource*>(swapchainTexture);

    D3D12_RESOURCE_BARRIER barriers[2] = {};
    barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Transition.pResource = midRes;
    barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

    barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[1].Transition.pResource = dstRes;
    barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

    m_vrCmdList->ResourceBarrier(2, barriers);

    D3D12_RESOURCE_DESC srcDesc = midRes->GetDesc();
    D3D12_RESOURCE_DESC dstDesc = dstRes->GetDesc();

    if (m_d3d11On12Device && params && params->focalLength > 0.0f) {
        // Run stereo reprojection via D3D11On12
        // We first transition the resources to D3D11On12 compatible states
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        m_vrCmdList->ResourceBarrier(2, barriers);

        // Execute DX12 command list to apply transitions before giving to D3D11On12
        m_vrCmdList->Close();
        ID3D12CommandList* lists[] = { m_vrCmdList };
        m_vrCommandQueue->ExecuteCommandLists(1, lists);
        
        // Setup D3D11On12 wrapped resources
        D3D11_RESOURCE_FLAGS d3d11FlagsSrc = { D3D11_BIND_SHADER_RESOURCE };
        D3D11_RESOURCE_FLAGS d3d11FlagsDst = { D3D11_BIND_RENDER_TARGET };
        ID3D11Resource* wrappedMidRes = nullptr;
        ID3D11Resource* wrappedDstRes = nullptr;
        
        m_d3d11On12Device->CreateWrappedResource(midRes, &d3d11FlagsSrc, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, IID_PPV_ARGS(&wrappedMidRes));
        m_d3d11On12Device->CreateWrappedResource(dstRes, &d3d11FlagsDst, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET, IID_PPV_ARGS(&wrappedDstRes));

        if (wrappedMidRes && wrappedDstRes) {
            m_d3d11On12Device->AcquireWrappedResources(&wrappedMidRes, 1);
            m_d3d11On12Device->AcquireWrappedResources(&wrappedDstRes, 1);

            ID3D11ShaderResourceView* midSRV = nullptr;
            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // Fallback default
            
            D3D12_RESOURCE_DESC mDesc = midRes->GetDesc();
            if (mDesc.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS || mDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            else if (mDesc.Format == DXGI_FORMAT_B8G8R8A8_TYPELESS || mDesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) srvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            else srvDesc.Format = mDesc.Format;

            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;
            srvDesc.Texture2D.MostDetailedMip = 0;

            m_d3d11Device->CreateShaderResourceView(wrappedMidRes, &srvDesc, &midSRV);
            
            // Dummy depth for now
            m_stereoPipeline.RenderComputeOnly(m_d3d11Context, midSRV, nullptr, *params);
            
            // Copy right eye to destination swapchain
            ID3D11Resource* rightEyeRes = nullptr;
            if (m_stereoPipeline.GetRightEyeSRV()) {
                m_stereoPipeline.GetRightEyeSRV()->GetResource(&rightEyeRes);
                m_d3d11Context->CopyResource(wrappedDstRes, rightEyeRes);
                rightEyeRes->Release();
            }

            if (midSRV) midSRV->Release();

            m_d3d11On12Device->ReleaseWrappedResources(&wrappedMidRes, 1);
            m_d3d11On12Device->ReleaseWrappedResources(&wrappedDstRes, 1);
            m_d3d11Context->Flush();
        }
        
        if (wrappedMidRes) wrappedMidRes->Release();
        if (wrappedDstRes) wrappedDstRes->Release();

        // Reopen cmd list to transition midRes back to COMMON.
        // OpenXR expects dstRes (swapchain texture) to be left in D3D12_RESOURCE_STATE_RENDER_TARGET state!
        m_vrCmdAlloc[m_vrFrameIndex]->Reset();
        m_vrCmdList->Reset(m_vrCmdAlloc[m_vrFrameIndex], nullptr);
        
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
        m_vrCmdList->ResourceBarrier(1, barriers);
    } else {
        // Fallback flat 2D copy
        D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
        dstLoc.pResource = dstRes;
        dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLoc.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
        srcLoc.pResource = midRes;
        srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        srcLoc.SubresourceIndex = 0;

        D3D12_BOX srcBox = {};
        srcBox.left = 0;
        srcBox.top = 0;
        srcBox.front = 0;
        srcBox.right = (std::min)(static_cast<UINT>(srcDesc.Width), static_cast<UINT>(dstDesc.Width));
        srcBox.bottom = (std::min)(srcDesc.Height, dstDesc.Height);
        srcBox.back = 1;

        m_vrCmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, &srcBox);

        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

        m_vrCmdList->ResourceBarrier(2, barriers);
    }

    m_vrCmdList->Close();
    ID3D12CommandList* lists[] = { m_vrCmdList };
    m_vrCommandQueue->ExecuteCommandLists(1, lists);
    m_vrCommandQueue->Signal(m_vrFence, newFenceValue);

    {
        std::lock_guard<std::mutex> lock(m_indexMutex);
        m_vrReadFenceValues[readIdx] = newFenceValue;
    }
}
