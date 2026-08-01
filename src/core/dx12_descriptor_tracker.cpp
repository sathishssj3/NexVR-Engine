#include "dx12_descriptor_tracker.h"

namespace vrinject {

void Dx12DescriptorTracker::PopulateMetadata(ID3D12Resource* resource, DescriptorRecord& outRecord, const D3D12_DEPTH_STENCIL_VIEW_DESC* pDesc) {
    if (!resource) return;
    
    D3D12_RESOURCE_DESC desc = resource->GetDesc();
    outRecord.width = static_cast<uint32_t>(desc.Width);
    outRecord.height = static_cast<uint32_t>(desc.Height);
    
    // If a view desc is provided and isn't UNKNOWN, it overrides the resource format.
    if (pDesc && pDesc->Format != DXGI_FORMAT_UNKNOWN) {
        outRecord.format = pDesc->Format;
    } else {
        outRecord.format = desc.Format;
    }
    
    outRecord.sampleCount = desc.SampleDesc.Count;
    outRecord.mipLevels = desc.MipLevels;
    
    if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D || desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D) {
        outRecord.arraySize = desc.DepthOrArraySize;
    } else {
        outRecord.arraySize = 1;
    }
}

void Dx12DescriptorTracker::RegisterDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE destHandle, ID3D12Resource* resource, const D3D12_DEPTH_STENCIL_VIEW_DESC* pDesc) {
    if (!resource) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    DescriptorRecord record{};
    record.generation = m_nextGeneration.fetch_add(1);
    record.handle = destHandle;
    record.resource = resource;
    record.resourceGeneration = 1; // Simplified for now, real epoch tracking can update this
    
    PopulateMetadata(resource, record, pDesc);
    
    m_descriptors[destHandle.ptr] = record;
    m_resourceToDescriptors[resource].push_back(destHandle.ptr);
}

void Dx12DescriptorTracker::CopyDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE destHandle, D3D12_CPU_DESCRIPTOR_HANDLE srcHandle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_descriptors.find(srcHandle.ptr);
    if (it != m_descriptors.end()) {
        DescriptorRecord newRecord = it->second;
        newRecord.generation = m_nextGeneration.fetch_add(1);
        newRecord.handle = destHandle;
        
        m_descriptors[destHandle.ptr] = newRecord;
        m_resourceToDescriptors[newRecord.resource].push_back(destHandle.ptr);
    } else {
        // If the source wasn't tracked (e.g. not a DSV), ensure we don't hold stale data at the destination.
        m_descriptors.erase(destHandle.ptr);
    }
}

bool Dx12DescriptorTracker::ResolveDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE handle, GraphicsResourceIdentity& outIdentity) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_descriptors.find(handle.ptr);
    if (it != m_descriptors.end()) {
        const DescriptorRecord& rec = it->second;
        
        outIdentity.backend = GraphicsBackend::DX12;
        outIdentity.nativeHandle = rec.resource;
        outIdentity.width = rec.width;
        outIdentity.height = rec.height;
        outIdentity.format = rec.format;
        outIdentity.sampleCount = rec.sampleCount;
        outIdentity.arraySize = rec.arraySize;
        outIdentity.mipLevels = rec.mipLevels;
        
        // Use the descriptor's generation combined with resource generation to form a unique epoch
        outIdentity.epoch.resourceGeneration = rec.generation; 
        
        return true;
    }
    
    return false;
}

void Dx12DescriptorTracker::OnResourceReleased(ID3D12Resource* resource) {
    if (!resource) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_resourceToDescriptors.find(resource);
    if (it != m_resourceToDescriptors.end()) {
        for (SIZE_T ptr : it->second) {
            // Only erase if the descriptor still points to THIS resource.
            // (ABA could mean a new resource was created and claimed the descriptor handle, 
            // though unlikely without freeing it first, we double check).
            auto descIt = m_descriptors.find(ptr);
            if (descIt != m_descriptors.end() && descIt->second.resource == resource) {
                m_descriptors.erase(descIt);
            }
        }
        m_resourceToDescriptors.erase(it);
    }
}

void Dx12DescriptorTracker::InvalidateRange(SIZE_T startPtr, SIZE_T endPtr) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto it = m_descriptors.begin(); it != m_descriptors.end(); ) {
        if (it->first >= startPtr && it->first < endPtr) {
            it = m_descriptors.erase(it);
            // We aren't cleaning up m_resourceToDescriptors here, which means it might leak some SIZE_T entries over time
            // if heaps are constantly recreated. For production we might want a reverse lookup cleanup.
        } else {
            ++it;
        }
    }
}

void Dx12DescriptorTracker::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_descriptors.clear();
    m_resourceToDescriptors.clear();
}

} // namespace vrinject
