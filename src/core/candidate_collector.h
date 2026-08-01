#pragma once
#include <d3d11.h>
#include "camera_candidate.h"
#include <vector>

namespace vrinject {

class CandidateCollector {
public:
    static CandidateCollector& Get() {
        static CandidateCollector instance;
        return instance;
    }

    // Generic push mechanism (for Vulkan, DX12, etc.)
    void SubmitCandidate(const CameraCandidate& candidate);

    // Legacy / DX11 helper
    void OnConstantBufferUpdate(ID3D11Buffer* buffer, const void* data, size_t byteSize);

    // Called at the end of the frame to get all candidates seen this frame
    std::vector<CameraCandidate> GetAndClearCandidates();

private:
    CandidateCollector() = default;
    
    std::vector<CameraCandidate> m_frameCandidates;
    // We would use a lock-free queue in production, but for now a simple pre-allocated vector or spinlock
    // To satisfy the "no blocking locks in Present" rule, we'll just use a small spinlock for the collector
    // since it runs on the render thread and is consumed by the render thread (Present)
};

} // namespace vrinject
