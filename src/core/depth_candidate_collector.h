#pragma once
#include "depth_candidate.h"
#include <mutex>
#include <unordered_map>
#include <vector>

namespace vrinject {

class DepthCandidateCollector {
public:
    static DepthCandidateCollector& Get() {
        static DepthCandidateCollector instance;
        return instance;
    }

    void OnDepthSurfaceCreated(void* resourcePointer, uint32_t width, uint32_t height, uint32_t format, uint32_t sampleCount, uint32_t arraySize, uint32_t mipLevels, uint32_t generation);
    void OnDepthSurfaceReleased(void* resourcePointer);
    
    void OnOMSetRenderTargets(void* depthResourcePointer);
    void OnOMSetRenderTargets(const GraphicsResourceIdentity& identity);
    void OnClearDepthStencilView(void* depthResourcePointer, float depth);
    void OnClearDepthStencilView(const GraphicsResourceIdentity& identity, float depth);
    
    void OnFrameEnd(uint64_t frameNumber);
    
    std::vector<DepthCandidate> GetCandidatesSnapshot() const;
    void Clear();

private:
    DepthCandidateCollector() = default;
    ~DepthCandidateCollector() = default;

    mutable std::mutex m_mutex;
    std::unordered_map<DepthResourceIdentity, DepthCandidate> m_candidates;
    
    // Quick lookup from pointer to identity (only tracks living resources to handle pointer reuse properly)
    std::unordered_map<void*, DepthResourceIdentity> m_livingResources;
};

} // namespace vrinject
