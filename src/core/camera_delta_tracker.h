#pragma once
#include "camera_candidate.h"
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <DirectXMath.h>

namespace vrinject {

struct DynamicCandidate {
    uint8_t* staticPointer;
    bool isDoublePrecision;
    float temporalScore;
    Matrix4x4 previousMatrix;
    bool hasPrevious;
};

class CameraDeltaTracker {
public:
    CameraDeltaTracker() = default;

    // Pulls new candidates from MemoryScanner and tracks them over time
    void PollAndTrackCandidates();

    // Returns the highest confidence locked camera matrix
    bool GetLockedCamera(Matrix4x4& outMatrix);

private:
    std::unordered_map<uint8_t*, DynamicCandidate> m_candidates;
    uint8_t* m_lockedPointer = nullptr;
    
    // Float vs Double disambiguation (Gap 3)
    bool m_isEngineUE5 = false; 

    float CalculateDelta(const Matrix4x4& m1, const Matrix4x4& m2) const;
    void UpdateCandidateMotion(DynamicCandidate& candidate, const Matrix4x4& currentMatrix, float mouseDeltaX, float mouseDeltaY);
    
    // Helper to read float or double matrix safely
    bool SafeReadMatrix(uint8_t* address, bool isDouble, Matrix4x4& outMatrix);
};

} // namespace vrinject
