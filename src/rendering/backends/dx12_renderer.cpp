#include "dx12_renderer.h"
#include <vector>
#include <string>
#include <windows.h>
#include <algorithm>
#include <d3dcompiler.h>
#include "../../core/logger.h"
#include <bcrypt.h>
#ifdef _MSC_VER
#pragma comment(lib, "bcrypt.lib")
#endif

#ifndef EXPECTED_SHADER_HASH
#define EXPECTED_SHADER_HASH L"SECURITY_ERROR: PLEASE_SET_EXPECTED_SHADER_HASH_IN_BUILD_CONFIG"
#endif

// FIX #23: Use the module handle stored at DLL attach time instead of
// searching by name, so path resolution works when the DLL is renamed
// (e.g. dxgi.dll proxy mode or any other deployment name).
extern HMODULE g_hModule;

namespace {
std::wstring ComputeShaderHashSHA256(const uint8_t* data, size_t size) {
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    std::wstring hashResult = L"";
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0) != 0) return L"";

    DWORD cbData = 0, cbHashObject = 0;
    if (BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&cbHashObject, sizeof(DWORD), &cbData, 0) == 0) {
        std::vector<BYTE> pbHashObject(cbHashObject);
        DWORD cbHash = 0;
        if (BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&cbHash, sizeof(DWORD), &cbData, 0) == 0) {
            std::vector<BYTE> pbHash(cbHash);
            if (BCryptCreateHash(hAlg, &hHash, pbHashObject.data(), cbHashObject, NULL, 0, 0) == 0) {
                BCryptHashData(hHash, (PUCHAR)data, (ULONG)size, 0);
                BCryptFinishHash(hHash, pbHash.data(), cbHash, 0);
                
                wchar_t hexChar[3];
                for (DWORD i = 0; i < cbHash; ++i) {
                    swprintf_s(hexChar, L"%02x", pbHash[i]);
                    hashResult += hexChar;
                }
            }
        }
    }
    if (hHash) BCryptDestroyHash(hHash);
    if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
    return hashResult;
}
}



bool DX12Renderer::WaitForFence(ID3D12Fence* fence, uint64_t value, HANDLE eventHandle, const char* context) {
    if (!fence || !eventHandle || value == 0 || fence->GetCompletedValue() >= value) return true;

    const HRESULT hr = fence->SetEventOnCompletion(value, eventHandle);
    if (FAILED(hr)) {
        LOG_ERROR("DX12Renderer: SetEventOnCompletion failed in %s (HR=0x%08X)", context, hr);
        return false;
    }

    const DWORD waitResult = WaitForSingleObject(eventHandle, 2000);
    if (waitResult != WAIT_OBJECT_0) {
        const HRESULT removedReason = m_device ? m_device->GetDeviceRemovedReason() : E_FAIL;
        LOG_ERROR(
            "DX12Renderer: fence wait failed/timed out in %s (wait=%lu, removed=0x%08X)",
            context,
            waitResult,
            removedReason);
        return false;
    }
    return true;
}

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
        Shutdown();
        return false;
    }

    // Worker fence — used to synchronize our worker queue's command allocator reuse
    if (FAILED(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_syncFence)))) {
        Shutdown();
        return false;
    }
    if (FAILED(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_vrFence)))) {
        Shutdown();
        return false;
    }
    m_syncFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_syncFenceEvent) {
        Shutdown();
        return false;
    }
    m_vrFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_vrFenceEvent) {
        Shutdown();
        return false;
    }

    // Command allocators and list for our worker queue
    for (int i = 0; i < 2; ++i) {
        if (FAILED(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_cmdAlloc[i])))) {
            Shutdown();
            return false;
        }
        if (FAILED(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_vrCmdAlloc[i])))) {
            Shutdown();
            return false;
        }
    }
    
    if (FAILED(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_cmdAlloc[0], nullptr, IID_PPV_ARGS(&m_cmdList)))) {
        Shutdown();
        return false;
    }
    if (FAILED(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_vrCmdAlloc[0], nullptr, IID_PPV_ARGS(&m_vrCmdList)))) {
        Shutdown();
        return false;
    }
    m_cmdList->Close();
    m_vrCmdList->Close();

    // Create descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = 1024;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_srvUavHeap)))) {
        Shutdown();
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
    // D3D11On12 removed

    if (m_device) { m_device->Release(); m_device = nullptr; }
}

void DX12Renderer::SetVRResolutionAndFormat(uint32_t width, uint32_t height, int64_t format) {
    m_vrWidth = width;
    m_vrHeight = height;
    m_vrFormat = format;
    
    m_vrWidth = width;
    m_vrHeight = height;
    m_vrFormat = format;
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
        if (!WaitForFence(m_syncFence, maxSync, m_syncFenceEvent, "Flush(sync)")) return;
    }
    
    if (m_vrFence) {
        uint64_t maxVr = m_vrReadFenceValues[0] > m_vrReadFenceValues[1] ? m_vrReadFenceValues[0] : m_vrReadFenceValues[1];
        if (!WaitForFence(m_vrFence, maxVr, m_vrFenceEvent, "Flush(vr)")) return;
    }
}



ShaderHandle DX12Renderer::LoadComputeShader(const uint8_t* bytecode, size_t bytecodeSize) {
    ShaderHandle handle = {};
    if (!m_device || !bytecode || bytecodeSize == 0) return handle;

    std::wstring expectedHash = EXPECTED_SHADER_HASH;
    if (expectedHash == L"SECURITY_ERROR: PLEASE_SET_EXPECTED_SHADER_HASH_IN_BUILD_CONFIG") {
        LOG_ERROR("SECURITY ERROR: Shader hash not configured! Set EXPECTED_SHADER_HASH to the SHA-256 of the expected shader.");
        return handle;
    }

#ifndef _DEBUG
    if (expectedHash.empty()) {
        LOG_ERROR("SECURITY ERROR: DX12 shader verification is disabled in Release build! EXPECTED_SHADER_HASH must not be empty.");
        return handle;
    }
#endif

    if (!expectedHash.empty()) {
        std::wstring actualHash = ComputeShaderHashSHA256(bytecode, bytecodeSize);
        auto toLower = [](std::wstring s) {
            for (auto& c : s) {
                if (c >= L'A' && c <= L'Z') c = c - L'A' + L'a';
            }
            return s;
        };
        if (actualHash.empty() || toLower(actualHash) != toLower(expectedHash)) {
            LOG_ERROR("DX12 Shader bytecode integrity check failed (SHA-256 mismatch)");
            return handle;
        }
    }

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
    HRESULT serializeHR = D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    // FIX #15: Always release errorBlob — it can be non-null even on success (warnings).
    if (errorBlob) { errorBlob->Release(); errorBlob = nullptr; }
    if (FAILED(serializeHR)) {
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
    if (!WaitForFence(m_syncFence, fenceToWaitFor, m_syncFenceEvent, "ExecuteTonemap allocator")) return;
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
    // FIX #2: Acquire m_vrQueueMutex BEFORE calling WaitForFence to ensure
    // SkipVRFrame and CopyToSwapchainVR never race to Signal the vrFence,
    // which would break monotonic fence value ordering and cause a TDR.
    std::lock_guard<std::mutex> vrLock(m_vrQueueMutex);

    int readIdx;
    uint64_t newFenceValue;
    {
        std::lock_guard<std::mutex> lock(m_indexMutex);
        readIdx = m_readIndex.load();
        m_vrFenceValue++;
        newFenceValue = m_vrFenceValue;
    }
    
    if (!m_device || !m_vrCommandQueue) return;

    // Wait for the previous signal to complete before issuing the next one.
    if (!WaitForFence(m_vrFence, newFenceValue - 1, m_vrFenceEvent, "SkipVRFrame ordering")) return;

    m_vrCommandQueue->Signal(m_vrFence, newFenceValue);
    
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

    if (!WaitForFence(m_vrFence, newFenceValue - 1, m_vrFenceEvent, "CopyToSwapchain ordering")) return;

    std::lock_guard<std::mutex> vrLock(m_vrQueueMutex);

    m_vrFrameIndex = (m_vrFrameIndex + 1) % 2;
    uint64_t vrFenceToWaitFor = m_vrAllocatorFenceValues[m_vrFrameIndex];
    if (!WaitForFence(m_vrFence, vrFenceToWaitFor, m_vrFenceEvent, "CopyToSwapchain allocator")) return;
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
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

    m_vrCmdList->ResourceBarrier(2, barriers);

    D3D12_RESOURCE_DESC srcDesc = midRes->GetDesc();
    D3D12_RESOURCE_DESC dstDesc = dstRes->GetDesc();

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

    m_vrCmdList->Close();
    ID3D12CommandList* lists[] = { m_vrCmdList };
    m_vrCommandQueue->ExecuteCommandLists(1, lists);
    m_vrCommandQueue->Signal(m_vrFence, newFenceValue);
    m_vrAllocatorFenceValues[m_vrFrameIndex] = newFenceValue;

    {
        std::lock_guard<std::mutex> lock(m_indexMutex);
        m_vrReadFenceValues[readIdx] = newFenceValue;
    }
}
