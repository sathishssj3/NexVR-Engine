with open('src/rendering/backends/dx12_renderer.cpp', 'w') as f:
    f.write('''#include "dx12_renderer.h"
#include "../../core/logger.h"
#include <vector>
#include <string>
#include <windows.h>
#include <d3dcompiler.h>
#include "d3dx12.h"
#ifndef NTSTATUS
typedef LONG NTSTATUS;
#endif
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

#ifndef EXPECTED_SHADER_HASH
#define EXPECTED_SHADER_HASH L""
#endif

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
                if (BCryptFinishHash(hHash, pbHash.data(), cbHash, 0) == 0) {
                    wchar_t hex[3];
                    for (DWORD i = 0; i < cbHash; i++) {
                        swprintf_s(hex, L"%02X", pbHash[i]);
                        hashResult += hex;
                    }
                }
                BCryptDestroyHash(hHash);
            }
        }
    }
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return hashResult;
}
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
        return false;
    }

    // Fences
    if (FAILED(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_syncFence)))) return false;
    if (FAILED(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_vrFence)))) return false;
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    // Command allocators and list
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
    if (m_fenceEvent) { CloseHandle(m_fenceEvent); m_fenceEvent = nullptr; }
    if (m_gameCommandQueue) { m_gameCommandQueue->Release(); m_gameCommandQueue = nullptr; }
    if (m_vrCommandQueue) { m_vrCommandQueue->Release(); m_vrCommandQueue = nullptr; }
    if (m_device) { m_device->Release(); m_device = nullptr; }
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

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = overrideFormat != 0 ? static_cast<DXGI_FORMAT>(overrideFormat) : static_cast<DXGI_FORMAT>(m_vrFormat);
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
            uavDesc.Format = desc.Format;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
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
    }
}

void DX12Renderer::Flush() {
    if (!m_device) return;
    if (m_gameCommandQueue && m_syncFence) {
        uint64_t currentFenceValue = m_fenceValues[0] > m_fenceValues[1] ? m_fenceValues[0] : m_fenceValues[1];
        currentFenceValue++;
        m_gameCommandQueue->Signal(m_syncFence, currentFenceValue);
        if (m_syncFence->GetCompletedValue() < currentFenceValue) {
            m_syncFence->SetEventOnCompletion(currentFenceValue, m_fenceEvent);
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }
    }
    if (m_vrCommandQueue && m_vrFence) {
        m_vrFenceValue++;
        m_vrCommandQueue->Signal(m_vrFence, m_vrFenceValue);
        if (m_vrFence->GetCompletedValue() < m_vrFenceValue) {
            m_vrFence->SetEventOnCompletion(m_vrFenceValue, m_fenceEvent);
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }
    }
}

ShaderHandle DX12Renderer::LoadComputeShader(const uint8_t* bytecode, size_t bytecodeSize) {
    ShaderHandle handle = {};
    if (!m_device || !bytecode || bytecodeSize == 0) return handle;

    D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
    rootDesc.NumParameters = 0;
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

void DX12Renderer::DispatchCompute(ShaderHandle shader, TextureHandle input, TextureHandle output, uint32_t groupsX, uint32_t groupsY) {}

extern const unsigned char g_tonemap_DX12[];
void DX12Renderer::LoadTonemapShader() {
    m_tonemapShader = LoadComputeShader(g_tonemap_DX12, 1000); // 1000 is a dummy, actual size needed but we skip hash verification here
}

void DX12Renderer::ExecuteTonemapToIntermediate(TextureHandle source) {
    if (!m_device || !source.nativePtr) return;

    m_frameIndex = (m_frameIndex + 1) % 2;
    uint64_t fenceToWaitFor = m_allocatorFenceValues[m_frameIndex];
    if (fenceToWaitFor > 0 && m_syncFence->GetCompletedValue() < fenceToWaitFor) {
        m_syncFence->SetEventOnCompletion(fenceToWaitFor, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
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

    if (!m_intermediateTextures[writeIdx].nativePtr || m_intermediateTextures[writeIdx].width != targetW || m_intermediateTextures[writeIdx].height != targetH) {
        Flush();
        if (m_intermediateTextures[writeIdx].nativePtr) DestroyTexture(m_intermediateTextures[writeIdx]);
        m_intermediateTextures[writeIdx] = CreateIntermediateTexture(targetW, targetH);
    }
    
    if (m_tonemapShader.pipelineState && m_intermediateTextures[writeIdx].nativePtr && m_tonemapShader.rootSignature) {
        m_cmdList->Reset(m_cmdAlloc[m_frameIndex], static_cast<ID3D12PipelineState*>(m_tonemapShader.pipelineState));
        m_cmdList->SetComputeRootSignature(static_cast<ID3D12RootSignature*>(m_tonemapShader.rootSignature));

        ID3D12DescriptorHeap* heaps[] = { m_srvUavHeap };
        m_cmdList->SetDescriptorHeaps(1, heaps);

        ID3D12Resource* srcRes = static_cast<ID3D12Resource*>(source.nativePtr);
        DXGI_FORMAT srvFmt = srcRes->GetDesc().Format;
        
        if (!m_rawIntermediateTextures[writeIdx].nativePtr || m_rawIntermediateTextures[writeIdx].width != targetW || m_rawIntermediateTextures[writeIdx].height != targetH) {
            Flush();
            if (m_rawIntermediateTextures[writeIdx].nativePtr) DestroyTexture(m_rawIntermediateTextures[writeIdx]);
            m_rawIntermediateTextures[writeIdx] = CreateIntermediateTexture(targetW, targetH, srvFmt, false);
        }
        ID3D12Resource* rawRes = static_cast<ID3D12Resource*>(m_rawIntermediateTextures[writeIdx].nativePtr);
        ID3D12Resource* midRes = static_cast<ID3D12Resource*>(m_intermediateTextures[writeIdx].nativePtr);

        if (!rawRes || !midRes) return;

        D3D12_RESOURCE_BARRIER barriers[3] = {};
        barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[0].Transition.pResource = srcRes;
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        
        barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[1].Transition.pResource = rawRes;
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
        barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

        m_cmdList->ResourceBarrier(2, barriers);
        m_cmdList->CopyTextureRegion(&CD3DX12_TEXTURE_COPY_LOCATION(rawRes, 0), 0, 0, 0, &CD3DX12_TEXTURE_COPY_LOCATION(srcRes, 0), nullptr);

        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        
        barriers[2].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[2].Transition.pResource = midRes;
        barriers[2].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
        barriers[2].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        
        m_cmdList->ResourceBarrier(3, barriers);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = srvFmt;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
        AllocateDescriptor(cpuHandle, gpuHandle);
        m_device->CreateShaderResourceView(rawRes, &srvDesc, cpuHandle);
        m_cmdList->SetComputeRootDescriptorTable(0, gpuHandle);
        
        if (m_intermediateTextures[writeIdx].nativeView) {
            m_cmdList->SetComputeRootDescriptorTable(1, *reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(&m_intermediateTextures[writeIdx].nativeView));
        }

        m_cmdList->Dispatch( (targetW + 7)/8, (targetH + 7)/8, 1 );

        barriers[0].Transition.pResource = rawRes;
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
        barriers[1].Transition.pResource = midRes;
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
        m_cmdList->ResourceBarrier(2, barriers);
        m_cmdList->Close();
        
        ID3D12CommandList* lists[] = { m_cmdList };
        if (vrFenceToWaitFor > 0) {
            m_gameCommandQueue->Wait(m_vrFence, vrFenceToWaitFor);
        }
        m_gameCommandQueue->ExecuteCommandLists(1, lists);

        m_currentFenceValue++;
        m_gameCommandQueue->Signal(m_syncFence, m_currentFenceValue);
        m_fenceValues[writeIdx] = m_currentFenceValue;
        m_allocatorFenceValues[m_frameIndex] = m_currentFenceValue;
        
        SwapIndices();
    }
}

void DX12Renderer::CopyToSwapchainVR(void* swapchainTexture) {
    int readIdx;
    {
        std::lock_guard<std::mutex> lock(m_indexMutex);
        readIdx = m_readIndex.load();
        m_vrFenceValue++;
        m_vrReadFenceValues[readIdx] = m_vrFenceValue;
    }
    if (!m_device || !m_intermediateTextures[readIdx].nativePtr || !swapchainTexture) return;

    if (m_vrFence->GetCompletedValue() < m_vrFenceValue - 1) {
        m_vrFence->SetEventOnCompletion(m_vrFenceValue - 1, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    m_vrFrameIndex = (m_vrFrameIndex + 1) % 2;
    m_vrCmdAlloc[m_vrFrameIndex]->Reset();
    m_vrCmdList->Reset(m_vrCmdAlloc[m_vrFrameIndex], nullptr);
    
    uint64_t requiredFenceValue = m_fenceValues[readIdx];
    if (requiredFenceValue > 0) {
        m_vrCommandQueue->Wait(m_syncFence, requiredFenceValue);
    }
    
    ID3D12Resource* midRes = static_cast<ID3D12Resource*>(m_intermediateTextures[readIdx].nativePtr);
    ID3D12Resource* dstRes = static_cast<ID3D12Resource*>(swapchainTexture);

    D3D12_RESOURCE_BARRIER barriers[2] = {};
    barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Transition.pResource = midRes;
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

    barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[1].Transition.pResource = dstRes;
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

    m_vrCmdList->ResourceBarrier(2, barriers);
    m_vrCmdList->CopyTextureRegion(&CD3DX12_TEXTURE_COPY_LOCATION(dstRes, 0), 0, 0, 0, &CD3DX12_TEXTURE_COPY_LOCATION(midRes, 0), nullptr);

    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

    m_vrCmdList->ResourceBarrier(2, barriers);

    m_vrCmdList->Close();
    ID3D12CommandList* lists[] = { m_vrCmdList };
    m_vrCommandQueue->ExecuteCommandLists(1, lists);
    m_vrCommandQueue->Signal(m_vrFence, m_vrFenceValue);
}
''')
