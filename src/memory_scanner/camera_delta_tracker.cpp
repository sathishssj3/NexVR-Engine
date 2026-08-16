#include "memory_scanner/camera_delta_tracker.h"
#include "memory_scanner/page_scanner.h"
#include "core/seh_shield.h"
#include "core/engine_detector.h"
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

    auto newPointers = PageScanner::Get().GetCandidateStaticPointers();
    
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

void CameraDeltaTracker::UpdateHeadsetPose(const XrPosef& pose) {
    m_cachedHeadsetPose = pose;
}

static Matrix4x4 Multiply(const Matrix4x4& a, const Matrix4x4& b) {
    Matrix4x4 result;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            result.m[i][j] = a.m[i][0] * b.m[0][j] +
                             a.m[i][1] * b.m[1][j] +
                             a.m[i][2] * b.m[2][j] +
                             a.m[i][3] * b.m[3][j];
        }
    }
    return result;
}

bool CameraDeltaTracker::GetLockedCamera(Matrix4x4& outMatrix) {
    if (!m_lockedPointer) return false;
    
    auto it = m_candidates.find(m_lockedPointer);
    if (it == m_candidates.end()) return false;

    if (!SafeReadMatrix(m_lockedPointer, it->second.isDoublePrecision, outMatrix)) {
        return false;
    }

    // Convert cached headset pose to Matrix4x4 view
    float qx = m_cachedHeadsetPose.orientation.x;
    float qy = m_cachedHeadsetPose.orientation.y;
    float qz = m_cachedHeadsetPose.orientation.z;
    float qw = m_cachedHeadsetPose.orientation.w;
    float tx = m_cachedHeadsetPose.position.x;
    float ty = m_cachedHeadsetPose.position.y;
    float tz = m_cachedHeadsetPose.position.z;

    // Apply basic coordinate handedness matching (assumes left-handed for now, will be overridden by pipeline if wrong)
    tz = -tz; qx = -qx; qy = -qy;

    float xx = qx * qx, yy = qy * qy, zz = qz * qz;
    float xy = qx * qy, xz = qx * qz, yz = qy * qz;
    float wx = qw * qx, wy = qw * qy, wz = qw * qz;

    Matrix4x4 r;
    r.m[0][0] = 1.0f - 2.0f * (yy + zz); r.m[0][1] = 2.0f * (xy + wz); r.m[0][2] = 2.0f * (xz - wy); r.m[0][3] = 0.0f;
    r.m[1][0] = 2.0f * (xy - wz); r.m[1][1] = 1.0f - 2.0f * (xx + zz); r.m[1][2] = 2.0f * (yz + wx); r.m[1][3] = 0.0f;
    r.m[2][0] = 2.0f * (xz + wy); r.m[2][1] = 2.0f * (yz - wx); r.m[2][2] = 1.0f - 2.0f * (xx + yy); r.m[2][3] = 0.0f;
    r.m[3][0] = 0.0f; r.m[3][1] = 0.0f; r.m[3][2] = 0.0f; r.m[3][3] = 1.0f;

    Matrix4x4 headsetView;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            headsetView.m[i][j] = r.m[j][i];
        }
        headsetView.m[i][3] = 0.0f;
    }
    headsetView.m[3][0] = -(headsetView.m[0][0] * tx + headsetView.m[1][0] * ty + headsetView.m[2][0] * tz);
    headsetView.m[3][1] = -(headsetView.m[0][1] * tx + headsetView.m[1][1] * ty + headsetView.m[2][1] * tz);
    headsetView.m[3][2] = -(headsetView.m[0][2] * tx + headsetView.m[1][2] * ty + headsetView.m[2][2] * tz);
    headsetView.m[3][3] = 1.0f;

    // Apply headset rotation to base game camera
    outMatrix = Multiply(outMatrix, headsetView);

    // Overwrite the game's dynamic memory so the engine renders the new orientation on the next frame
    uintptr_t dynamicHeapAddr = 0;
    if (seh::SafeReadMemory(m_lockedPointer, &dynamicHeapAddr, sizeof(dynamicHeapAddr)) && dynamicHeapAddr != 0) {
        if (it->second.isDoublePrecision) {
            double dmat[16];
            for (int r = 0; r < 4; ++r) {
                for (int c = 0; c < 4; ++c) {
                    dmat[r * 4 + c] = static_cast<double>(outMatrix.m[r][c]);
                }
            }
            seh::SafeWriteMemory(reinterpret_cast<void*>(dynamicHeapAddr), dmat, sizeof(dmat));
        } else {
            seh::SafeWriteMemory(reinterpret_cast<void*>(dynamicHeapAddr), &outMatrix, sizeof(Matrix4x4));
        }
    }

    return true;
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
