#include "camera_delta_tracker.h"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <iostream>

namespace vrinject {
namespace ai {

void CameraDeltaTracker::UpdateCandidates(const std::vector<std::pair<int, MatrixPrediction>>& predictions, const void* bufferData) {
    const uint8_t* rawData = static_cast<const uint8_t*>(bufferData);
    
    // First, decay existing tracking entries
    for (auto& pair : m_tracking) {
        pair.second.staticFrames++;
    }

    // Process new predictions
    for (const auto& pred : predictions) {
        int offset = pred.first;
        if (pred.second.type != MatrixType::PerspectiveProjection) continue;
        if (pred.second.confidence < 0.6f) continue;

        DirectX::XMFLOAT4X4 currentMatrix;
        std::memcpy(&currentMatrix, rawData + offset, sizeof(DirectX::XMFLOAT4X4));

        auto it = m_tracking.find(offset);
        if (it != m_tracking.end()) {
            // Update existing
            CameraDelta& deltaObj = it->second;
            deltaObj.previousMatrix = deltaObj.currentMatrix;
            deltaObj.currentMatrix = currentMatrix;
            deltaObj.updateCount++;
            deltaObj.staticFrames = 0;
            
            float delta = CalculateDelta(deltaObj.previousMatrix, deltaObj.currentMatrix);
            
            if (delta > 0.0001f && delta < 5.0f) {
                // Smooth movement -> Likely a real moving camera
                deltaObj.deltaScore += 1.0f;
            } else if (delta == 0.0f) {
                // Static matrix -> Likely a UI or inactive camera
                deltaObj.deltaScore -= 0.5f;
            } else if (delta >= 5.0f) {
                // Erratically jumping -> Likely random memory or teleport
                deltaObj.deltaScore -= 0.1f;
            }
            
            deltaObj.deltaScore = std::clamp(deltaObj.deltaScore, -5.0f, 20.0f);
        } else {
            // New candidate
            CameraDelta newDelta = {};
            newDelta.currentMatrix = currentMatrix;
            newDelta.previousMatrix = currentMatrix;
            newDelta.deltaScore = 0.0f;
            newDelta.updateCount = 1;
            newDelta.staticFrames = 0;
            m_tracking[offset] = newDelta;
        }
    }

    // Find the best lock
    m_lockedOffset = -1;
    float bestScore = 5.0f; // Minimum score to achieve lock

    for (const auto& pair : m_tracking) {
        if (pair.second.staticFrames > 10) continue; // Ignore dead tracking
        if (pair.second.deltaScore > bestScore) {
            bestScore = pair.second.deltaScore;
            m_lockedOffset = pair.first;
            m_lockedMatrix = pair.second.currentMatrix;
        }
    }
}

bool CameraDeltaTracker::GetBestCamera(int& outOffset, DirectX::XMFLOAT4X4& outMatrix) const {
    if (m_lockedOffset != -1) {
        outOffset = m_lockedOffset;
        outMatrix = m_lockedMatrix;
        return true;
    }
    return false;
}

void CameraDeltaTracker::Reset() {
    m_tracking.clear();
    m_lockedOffset = -1;
}

float CameraDeltaTracker::CalculateDelta(const DirectX::XMFLOAT4X4& m1, const DirectX::XMFLOAT4X4& m2) const {
    float delta = 0.0f;
    const float* a = &m1._11;
    const float* b = &m2._11;
    for (int i = 0; i < 16; ++i) {
        delta += std::abs(a[i] - b[i]);
    }
    return delta;
}

} // namespace ai
} // namespace vrinject
