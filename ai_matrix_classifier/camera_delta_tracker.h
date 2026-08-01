#pragma once
#include <DirectXMath.h>
#include <unordered_map>
#include <vector>
#include "matrix_classifier.h"

namespace vrinject {
namespace ai {

struct CameraDelta {
    DirectX::XMFLOAT4X4 currentMatrix;
    DirectX::XMFLOAT4X4 previousMatrix;
    float deltaScore;
    int updateCount;
    int staticFrames;
};

class CameraDeltaTracker {
public:
    CameraDeltaTracker() = default;

    // Updates tracking history based on the latest AI predictions
    void UpdateCandidates(const std::vector<std::pair<int, MatrixPrediction>>& predictions, const void* bufferData);

    // Returns true if a high-confidence camera matrix has been locked
    bool GetBestCamera(int& outOffset, DirectX::XMFLOAT4X4& outMatrix) const;

    void Reset();

private:
    std::unordered_map<int, CameraDelta> m_tracking;
    int m_lockedOffset = -1;
    DirectX::XMFLOAT4X4 m_lockedMatrix;
    float CalculateDelta(const DirectX::XMFLOAT4X4& m1, const DirectX::XMFLOAT4X4& m2) const;
};

} // namespace ai
} // namespace vrinject
