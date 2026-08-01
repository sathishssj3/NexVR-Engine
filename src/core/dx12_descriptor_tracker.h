#pragma once
#include <d3d12.h>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include "graphics_types.h"

namespace vrinject {

struct DescriptorRecord {
    uint64_t generation;
    D3D12_CPU_DESCRIPTOR_HANDLE handle;
    ID3D12Resource* resource;
    // We also store the basic identity so we can quickly construct GraphicsResourceIdentity
    uint32_t width;
    uint32_t height;
    int64_t format;
    uint32_t sampleCount;
    uint32_t arraySize;
    uint32_t mipLevels;
    uint64_t resourceGeneration; // Keep track of the resource epoch generation
};

class Dx12DescriptorTracker {
public:
    static Dx12DescriptorTracker& Get() {
        static Dx12DescriptorTracker instance;
        return instance;
    }

    // Called on CreateDepthStencilView
    void RegisterDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE destHandle, ID3D12Resource* resource, const D3D12_DEPTH_STENCIL_VIEW_DESC* pDesc);

    // Called on CopyDescriptors and CopyDescriptorsSimple
    void CopyDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE destHandle, D3D12_CPU_DESCRIPTOR_HANDLE srcHandle);

    // Retrieve full resource identity for OMSetRenderTargets
    bool ResolveDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE handle, GraphicsResourceIdentity& outIdentity);

    // Handle Resource Release to prevent stale pointers and UAF
    void OnResourceReleased(ID3D12Resource* resource);

    // Invalidate descriptors mapped to a destroyed heap
    // Note: Heap destruction might be hard to hook cleanly if it's implicitly destroyed.
    // Instead, ABA issues are mostly handled by generation tracking. We can expose an invalidation by range if needed.
    void InvalidateRange(SIZE_T startPtr, SIZE_T endPtr);

    void Clear();

private:
    Dx12DescriptorTracker() = default;
    ~Dx12DescriptorTracker() = default;

    // Fills in Resource metadata using ID3D12Resource::GetDesc
    void PopulateMetadata(ID3D12Resource* resource, DescriptorRecord& outRecord, const D3D12_DEPTH_STENCIL_VIEW_DESC* pDesc);

    std::mutex m_mutex;
    // Map of descriptor handle (ptr) -> Record
    std::unordered_map<SIZE_T, DescriptorRecord> m_descriptors;
    
    // Map of Resource -> list of descriptor handles (for fast invalidation)
    std::unordered_map<ID3D12Resource*, std::vector<SIZE_T>> m_resourceToDescriptors;

    std::atomic<uint64_t> m_nextGeneration{1};
};

} // namespace vrinject
