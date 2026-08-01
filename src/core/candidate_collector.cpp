#include "candidate_collector.h"
#include "camera_classifier.h"

namespace vrinject {

void CandidateCollector::OnConstantBufferUpdate(ID3D11Buffer* buffer, const void* data, size_t byteSize) {
    if (byteSize < sizeof(Matrix4x4)) return;

    // Scan the buffer for potential matrices
    const size_t numMatrices = byteSize / sizeof(Matrix4x4);
    const Matrix4x4* matrices = reinterpret_cast<const Matrix4x4*>(data);

    for (size_t i = 0; i < numMatrices; ++i) {
        CameraCandidate candidate;
        // Use the buffer address as ID for tracking (or hash of buffer ptr + offset)
        uint64_t id = reinterpret_cast<uint64_t>(buffer) + (i * sizeof(Matrix4x4));

        if (CameraClassifier::TryCreateCandidate(id, buffer, matrices[i], candidate)) {
            // Found a valid-looking projection/VP matrix
            m_frameCandidates.push_back(candidate);
        }
    }
}

void CandidateCollector::SubmitCandidate(const CameraCandidate& candidate) {
    m_frameCandidates.push_back(candidate);
}

std::vector<CameraCandidate> CandidateCollector::GetAndClearCandidates() {
    std::vector<CameraCandidate> copy = std::move(m_frameCandidates);
    m_frameCandidates.clear();
    return copy;
}

} // namespace vrinject
