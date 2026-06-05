#include "dx12_renderer.h"
#include "../../core/logger.h"
#include <vector>
#include <string>
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
    
    // In DX12, nativeContext from swapchain is usually the CommandQueue used for Present
    m_device = static_cast<ID3D12Device*>(nativeDevice);
    m_commandQueue = static_cast<ID3D12CommandQueue*>(nativeContext);
    
    m_device->AddRef();
    m_commandQueue->AddRef();

    // Create simple command allocators and list for copies/dispatches
    for (int i = 0; i < 2; ++i) {
        if (FAILED(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_cmdAlloc[i])))) {
            return false;
        }
    }
    
    if (FAILED(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_cmdAlloc[0], nullptr, IID_PPV_ARGS(&m_cmdList)))) {
        return false;
    }
    m_cmdList->Close(); // Close it initially

    // Create descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = 1024;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_srvUavHeap)))) {
        return false;
    }
    m_srvUavDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    return true;
}

void DX12Renderer::Shutdown() {
    if (m_cmdList) { m_cmdList->Release(); m_cmdList = nullptr; }
    for (int i = 0; i < 2; ++i) {
        if (m_cmdAlloc[i]) { m_cmdAlloc[i]->Release(); m_cmdAlloc[i] = nullptr; }
    }
    if (m_srvUavHeap) { m_srvUavHeap->Release(); m_srvUavHeap = nullptr; }
    if (m_commandQueue) { m_commandQueue->Release(); m_commandQueue = nullptr; }
    if (m_device) { m_device->Release(); m_device = nullptr; }
}

void DX12Renderer::AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE& cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE& gpuHandle) {
    if (m_descriptorCount >= 1024) m_descriptorCount = 0; // Wrap around for prototype
    
    cpuHandle = m_srvUavHeap->GetCPUDescriptorHandleForHeapStart();
    cpuHandle.ptr += m_descriptorCount * m_srvUavDescriptorSize;
    
    gpuHandle = m_srvUavHeap->GetGPUDescriptorHandleForHeapStart();
    gpuHandle.ptr += m_descriptorCount * m_srvUavDescriptorSize;
    
    m_descriptorCount++;
}

TextureHandle DX12Renderer::CreateTexture(uint32_t width, uint32_t height, uint32_t format, bool uav) {
    TextureHandle handle = {};
    if (!m_device) return handle;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = static_cast<DXGI_FORMAT>(format);
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = uav ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    ID3D12Resource* tex = nullptr;
    if (SUCCEEDED(m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&tex)))) {
        handle.nativePtr = tex;
        handle.width = width;
        handle.height = height;

        if (uav) {
            D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
            D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
            AllocateDescriptor(cpuHandle, gpuHandle);
            
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.Format = desc.Format;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            m_device->CreateUnorderedAccessView(tex, nullptr, &uavDesc, cpuHandle);
            
            // Store GPU handle ptr as nativeView for bind
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
    // Descriptors are ring-buffered in this prototype, no explicit free
}

ShaderHandle DX12Renderer::LoadComputeShader(const uint8_t* bytecode, size_t bytecodeSize) {
    ShaderHandle handle = {};
    if (!m_device || !bytecode || bytecodeSize == 0) return handle;

    // S5.2: Cryptographic Shader Verification
    std::wstring expectedHash = EXPECTED_SHADER_HASH;
    if (!expectedHash.empty()) {
        std::wstring actualHash = ComputeShaderHashSHA256(bytecode, bytecodeSize);
        if (actualHash.empty() || _wcsicmp(actualHash.c_str(), expectedHash.c_str()) != 0) {
            LOG_ERROR("DX12 Shader bytecode integrity check failed (SHA-256 mismatch)");
            return handle;
        }
    }

    // Create a dummy root signature for the prototype
    D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
    rootDesc.NumParameters = 0;
    rootDesc.pParameters = nullptr;
    rootDesc.NumStaticSamplers = 0;
    rootDesc.pStaticSamplers = nullptr;
    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ID3DBlob* signatureBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    // Note: D3D12SerializeRootSignature requires d3d12.lib
    // For simplicity, we assume we might serialize it or pass a pre-built one.
    // We will leave the Root Signature creation as a stub for the full implementation.
    
    // Create PSO
    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.CS.pShaderBytecode = bytecode;
    psoDesc.CS.BytecodeLength = bytecodeSize;
    // psoDesc.pRootSignature = ...

    ID3D12PipelineState* pso = nullptr;
    if (SUCCEEDED(m_device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pso)))) {
        handle.pipelineState = pso;
    }

    return handle;
}

void DX12Renderer::DestroyShader(ShaderHandle& handle) {
    if (handle.pipelineState) {
        static_cast<ID3D12PipelineState*>(handle.pipelineState)->Release();
        handle.pipelineState = nullptr;
    }
    if (handle.rootSignature) {
        static_cast<ID3D12RootSignature*>(handle.rootSignature)->Release();
        handle.rootSignature = nullptr;
    }
}

void DX12Renderer::DispatchCompute(ShaderHandle shader, TextureHandle input, TextureHandle output, uint32_t groupsX, uint32_t groupsY) {
    if (!m_device || !shader.pipelineState) return;

    m_frameIndex = (m_frameIndex + 1) % 2;
    m_cmdAlloc[m_frameIndex]->Reset();
    m_cmdList->Reset(m_cmdAlloc[m_frameIndex], static_cast<ID3D12PipelineState*>(shader.pipelineState));

    if (shader.rootSignature) {
        m_cmdList->SetComputeRootSignature(static_cast<ID3D12RootSignature*>(shader.rootSignature));
    }

    ID3D12DescriptorHeap* heaps[] = { m_srvUavHeap };
    m_cmdList->SetDescriptorHeaps(1, heaps);

    // Transition barriers for input/output would go here
    D3D12_RESOURCE_BARRIER barriers[2] = {};
    barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Transition.pResource = static_cast<ID3D12Resource*>(input.nativePtr);
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[1].Transition.pResource = static_cast<ID3D12Resource*>(output.nativePtr);
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    m_cmdList->ResourceBarrier(2, barriers);

    // Assuming Root Parameters 0 = SRV, 1 = UAV
    // (Needs actual descriptor handles from the texture creation)
    if (input.nativeView) {
        m_cmdList->SetComputeRootDescriptorTable(0, *reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(&input.nativeView));
    }
    if (output.nativeView) {
        m_cmdList->SetComputeRootDescriptorTable(1, *reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(&output.nativeView));
    }

    m_cmdList->Dispatch(groupsX, groupsY, 1);

    // Revert barriers
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
    m_cmdList->ResourceBarrier(2, barriers);

    m_cmdList->Close();
    ID3D12CommandList* lists[] = { m_cmdList };
    m_commandQueue->ExecuteCommandLists(1, lists);
}

void DX12Renderer::CopyToSwapchain(TextureHandle source, void* swapchainTexture) {
    if (!m_device || !source.nativePtr || !swapchainTexture) return;

    m_frameIndex = (m_frameIndex + 1) % 2;
    m_cmdAlloc[m_frameIndex]->Reset();
    m_cmdList->Reset(m_cmdAlloc[m_frameIndex], nullptr);

    ID3D12Resource* srcRes = static_cast<ID3D12Resource*>(source.nativePtr);
    ID3D12Resource* dstRes = static_cast<ID3D12Resource*>(swapchainTexture);

    D3D12_RESOURCE_BARRIER barriers[2] = {};
    barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Transition.pResource = srcRes;
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[1].Transition.pResource = dstRes;
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON; // Typical for OpenXR acquired images
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    m_cmdList->ResourceBarrier(2, barriers);

    m_cmdList->CopyResource(dstRes, srcRes);

    // Revert barriers so OpenXR can use them
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;

    m_cmdList->ResourceBarrier(2, barriers);

    m_cmdList->Close();
    ID3D12CommandList* lists[] = { m_cmdList };
    m_commandQueue->ExecuteCommandLists(1, lists);
}
