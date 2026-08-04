#include "camera_delta_tracker.h"
#include "memory_scanner.h"
#include "seh_shield.h"
#include "engine_detector.h"
#include <cmath>
#include <algorithm>

namespace vrinject {

bool CameraDeltaTracker::SafeReadMatrix(uint8_t* address, bool isDouble, Matrix4x4& outMatrix) {
    if (!address) return false;
    
    // The candidate pointer is a static pointer that points to the dynamic heap matrix.
    // First, safely read the dynamic pointer.
    uintptr_t dynamicHeapAddr = 0;
    if (!seh::SafeReadMemory(address, &dynamicHeapAddr, sizeof(dynamicHeapAddr))) return false;
    
    if (dynamicHeapAddr == 0) return false;

    if (isDouble) {
        double dmat[16];
        if (!seh::SafeReadMemory(reinterpret_cast<void*>(dynamicHeapAddr), dmat, sizeof(dmat))) return false;
        // Convert to float Matrix4x4
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                outMatrix.m[r][c] = static_cast<float>(dmat[r * 4 + c]);
            }
        }
    } else {
        if (!seh::SafeReadMemory(reinterpret_cast<void*>(dynamicHeapAddr), &outMatrix, sizeof(Matrix4x4))) return false;
    }
    return true;
}

void CameraDeltaTracker::PollAndTrackCandidates() {
    // Determine engine type once (Gap 3 Disambiguation)
    // Assume EngineDetector::Get().GetEngineType() exists and returns an enum. 
    // We'll hardcode checking against UE5 for now or use a heuristic if detector unavailable.
    m_isEngineUE5 = (EngineDetector::Get().GetEngineType() == EngineType::UnrealEngine5);

    auto newPointers = MemoryScanner::Get().GetCandidateStaticPointers();
    
    for (uint8_t* ptr : newPointers) {
        if (m_candidates.find(ptr) == m_candidates.end()) {
            DynamicCandidate c;
            c.staticPointer = ptr;
            // For now we assume if it's UE5 it's a double candidate, otherwise float.
            // Ideally MemoryScanner tags them, but we'll try reading both and validate.
            // Since the design says "disambiguate if both exist", we track them.
            c.isDoublePrecision = m_isEngineUE5; 
            c.temporalScore = 0.5f;
            c.hasPrevious = false;
            m_candidates[ptr] = c;
        }
    }

    // Dummy mouse deltas for correlation (normally fetched from raw input hooks)
    float mouseDeltaX = 1.0f; 
    float mouseDeltaY = 1.0f;

    uint8_t* bestPointer = nullptr;
    float bestScore = -1.0f;

    for (auto it = m_candidates.begin(); it != m_candidates.end(); ) {
        Matrix4x4 currentMatrix;
        if (!SafeReadMatrix(it->first, it->second.isDoublePrecision, currentMatrix)) {
            // Memory was freed or invalid (TOCTOU caught by SEH). Remove candidate.
            it = m_candidates.erase(it);
            continue;
        }

        UpdateCandidateMotion(it->second, currentMatrix, mouseDeltaX, mouseDeltaY);

        // Gap 3 Disambiguation:
        // If engine is explicitly UE5, and we have a double candidate, it gets a massive boost.
        float score = it->second.temporalScore;
        if (m_isEngineUE5 && it->second.isDoublePrecision) score += 10.0f;
        else if (!m_isEngineUE5 && !it->second.isDoublePrecision) score += 10.0f;

        if (score > bestScore) {
            bestScore = score;
            bestPointer = it->first;
        }
        ++it;
    }

    if (bestScore > 0.0f) {
        m_lockedPointer = bestPointer;
    }
}

bool CameraDeltaTracker::GetLockedCamera(Matrix4x4& outMatrix) {
    if (!m_lockedPointer) return false;
    
    auto it = m_candidates.find(m_lockedPointer);
    if (it == m_candidates.end()) return false;

    return SafeReadMatrix(m_lockedPointer, it->second.isDoublePrecision, outMatrix);
}

float CameraDeltaTracker::CalculateDelta(const Matrix4x4& m1, const Matrix4x4& m2) const {
    float diff = 0.0f;
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            diff += std::abs(m1.m[r][c] - m2.m[r][c]);
        }
    }
    return diff;
}

void CameraDeltaTracker::UpdateCandidateMotion(DynamicCandidate& candidate, const Matrix4x4& currentMatrix, float mouseDeltaX, float mouseDeltaY) {
    if (!candidate.hasPrevious) {
        candidate.previousMatrix = currentMatrix;
        candidate.hasPrevious = true;
        return;
    }

    float delta = CalculateDelta(candidate.previousMatrix, currentMatrix);
    candidate.previousMatrix = currentMatrix;
    
    // Correlate with physical input
    bool hasPhysicalMotion = (std::abs(mouseDeltaX) > 0.1f || std::abs(mouseDeltaY) > 0.1f);

    if (delta > 0.0001f && delta < 0.5f && hasPhysicalMotion) {
        candidate.temporalScore = (std::min)(candidate.temporalScore + 0.1f, 1.0f);
    } else if (delta > 0.0001f && !hasPhysicalMotion) {
        // Camera moved but player didn't input anything - could be cutscene or shadow map
        candidate.temporalScore = (std::max)(candidate.temporalScore - 0.05f, 0.0f);
    } else if (delta == 0.0f && !hasPhysicalMotion) {
        // Expected static state
    } else {
        candidate.temporalScore = (std::max)(candidate.temporalScore - 0.1f, 0.0f);
    }
}

} // namespace vrinject
