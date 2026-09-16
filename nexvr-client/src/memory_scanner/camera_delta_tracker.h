#pragma once
#include "heuristics/camera_candidate.h"
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <DirectXMath.h>
#include "openxr/openxr_types.h"

namespace vrinject {

struct DynamicCandidate {
    uint8_t* staticPointer = nullptr;
    bool isDoublePrecision = false;
    bool isViewMatrix = false;
    bool isProjectionMatrix = false;
    float temporalScore = 0.0f;
    Matrix4x4 previousMatrix = {};
    bool hasPrevious = false;
    uint32_t consecutiveMatches = 0;
};

class CameraDeltaTracker {
public:
    CameraDeltaTracker() = default;

    // Pulls new candidates from MemoryScanner and tracks them over time with physical/HMD input correlation
    void PollAndTrackCandidates();

    // Returns the highest confidence locked camera view matrix
    bool GetLockedCamera(Matrix4x4& outMatrix);

    // Returns both locked view and projection matrices
    bool GetLockedCamera(Matrix4x4& outView, Matrix4x4& outProj);

    // Checks if the camera is currently locked with high confidence
    bool IsLocked() const { return m_lockedPointer != nullptr && m_lockedConfidence >= 0.80f; }
    float GetConfidence() const { return m_lockedConfidence; }

    // Update the tracker with the latest headset pose
    void UpdateHeadsetPose(const XrPosef& pose);

private:
    std::unordered_map<uint8_t*, DynamicCandidate> m_candidates;
    uint8_t* m_lockedPointer = nullptr;
    uint8_t* m_lockedProjPointer = nullptr;
    float m_lockedConfidence = 0.0f;
    
    // Cached OpenXR pose to apply to the locked matrix
    XrPosef m_cachedHeadsetPose = {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}};
    XrPosef m_prevHeadsetPose = {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}};
    bool m_hasPrevHeadsetPose = false;
    
    // Float vs Double disambiguation (Gap 3)
    bool m_isEngineUE5 = false; 

    float CalculateDelta(const Matrix4x4& m1, const Matrix4x4& m2) const;
    float CalculateHmdAngularDelta(const XrPosef& p1, const XrPosef& p2) const;
    void UpdateCandidateMotion(DynamicCandidate& candidate, const Matrix4x4& currentMatrix, float inputMotionEnergy);
    
    // Helper to read float or double matrix safely
    bool SafeReadMatrix(uint8_t* address, bool isDouble, Matrix4x4& outMatrix);
    void FindMatchingProjection(uint8_t* viewAddress, bool isDouble);
};

} // namespace vrinject
