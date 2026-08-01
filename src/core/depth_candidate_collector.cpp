#include "depth_candidate_collector.h"

namespace vrinject {

void DepthCandidateCollector::OnDepthSurfaceCreated(void* resourcePointer, uint32_t width, uint32_t height, uint32_t format, uint32_t sampleCount, uint32_t arraySize, uint32_t mipLevels, uint32_t generation) {
    if (!resourcePointer) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    DepthResourceIdentity id;
    id.backend = GraphicsBackend::DX11; // Since we are currently relying on DX11 concepts here implicitly
    id.nativeHandle = resourcePointer;
    id.width = width;
    id.height = height;
    id.format = format;
    id.sampleCount = sampleCount;
    id.arraySize = arraySize;
    id.mipLevels = mipLevels;
    id.epoch.resourceGeneration = generation;
    
    // Register living resource mapping
    m_livingResources[resourcePointer] = id;
    
    // Create new candidate
    if (m_candidates.find(id) == m_candidates.end()) {
        DepthCandidate candidate;
        candidate.identity = id;
        m_candidates[id] = candidate;
    }
}

void DepthCandidateCollector::OnDepthSurfaceReleased(void* resourcePointer) {
    if (!resourcePointer) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_livingResources.find(resourcePointer);
    if (it != m_livingResources.end()) {
        m_livingResources.erase(it);
    }
}

void DepthCandidateCollector::OnOMSetRenderTargets(void* depthResourcePointer) {
    if (!depthResourcePointer) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_livingResources.find(depthResourcePointer);
    if (it != m_livingResources.end()) {
        auto candIt = m_candidates.find(it->second);
        if (candIt != m_candidates.end()) {
            candIt->second.bindCount++;
        }
    }
}

void DepthCandidateCollector::OnClearDepthStencilView(void* depthResourcePointer, float depth) {
    if (!depthResourcePointer) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_livingResources.find(depthResourcePointer);
    if (it != m_livingResources.end()) {
        auto candIt = m_candidates.find(it->second);
        if (candIt != m_candidates.end()) {
            candIt->second.clearCount++;
            candIt->second.lastClearDepth = depth;
        }
    }
}

void DepthCandidateCollector::OnOMSetRenderTargets(const GraphicsResourceIdentity& identity) {
    if (!identity.IsValid()) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Register living resource mapping implicitly if new or changed
    m_livingResources[identity.nativeHandle] = identity;
    
    // Create new candidate if necessary
    if (m_candidates.find(identity) == m_candidates.end()) {
        DepthCandidate candidate;
        candidate.identity = identity;
        m_candidates[identity] = candidate;
    }
    
    m_candidates[identity].bindCount++;
}

void DepthCandidateCollector::OnClearDepthStencilView(const GraphicsResourceIdentity& identity, float depth) {
    if (!identity.IsValid()) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Register living resource mapping implicitly if new or changed
    m_livingResources[identity.nativeHandle] = identity;
    
    // Create new candidate if necessary
    if (m_candidates.find(identity) == m_candidates.end()) {
        DepthCandidate candidate;
        candidate.identity = identity;
        m_candidates[identity] = candidate;
    }
    
    m_candidates[identity].clearCount++;
    m_candidates[identity].lastClearDepth = depth;
}

void DepthCandidateCollector::OnFrameEnd(uint64_t frameNumber) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Update frame seen counters and reset per-frame metrics where needed
    for (auto& pair : m_candidates) {
        if (pair.second.bindCount > 0 || pair.second.clearCount > 0) {
            pair.second.lastFrameSeen = pair.second.frameSeen;
            pair.second.frameSeen = frameNumber;
            pair.second.updateFrequency++;
        }
    }
}

std::vector<DepthCandidate> DepthCandidateCollector::GetCandidatesSnapshot() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<DepthCandidate> snapshot;
    snapshot.reserve(m_candidates.size());
    for (const auto& pair : m_candidates) {
        // If it's still alive (pointer didn't get reused)
        if (m_livingResources.find(pair.first.nativeHandle) != m_livingResources.end()) {
            if (m_livingResources.at(pair.first.nativeHandle) == pair.first) {
                snapshot.push_back(pair.second);
            }
        }
    }
    return snapshot;
}

void DepthCandidateCollector::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_candidates.clear();
    m_livingResources.clear();
}

} // namespace vrinject
