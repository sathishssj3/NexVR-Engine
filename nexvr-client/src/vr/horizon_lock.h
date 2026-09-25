#pragma once
#include "core/math_types.h"
#include <cmath>

namespace vrinject {

// P2: 6DOF Camera Decoupling & Horizon Lock
// Extracts the camera roll component from the view matrix and computes a
// correction angle that the shader uses to stabilize the horizon line.
// This prevents the virtual world from tilting when the game camera rolls
// (e.g., motorcycle lean in GTA V, explosion shakes in Sekiro).
class HorizonLock {
public:
    HorizonLock() = default;

    // Extract roll angle from a 4x4 view-projection matrix and compute
    // the correction needed to lock the horizon.
    // strength: blend factor [0, 1]. 0 = no correction, 1 = full lock.
    // Returns roll correction in radians.
    float ComputeRollCorrection(const Matrix4x4& viewMatrix, float strength) {
        if (strength < 0.001f) return 0.0f;

        // Extract camera right vector from view matrix (first row in row-major)
        float rightX = viewMatrix.m[0][0];
        float rightY = viewMatrix.m[0][1];

        // Roll angle = angle of the right vector projected onto the XY plane
        // relative to the world horizontal. atan2(y, x) gives the tilt.
        float rollRad = atan2f(rightY, rightX);

        // Apply dampened correction
        // Smooth the correction to avoid jitter on noisy camera matrices
        float targetCorrection = -rollRad * strength;

        // Exponential smoothing (tau ~60ms at 90fps = 5.4 frames)
        const float SMOOTHING = 0.15f;
        m_smoothedCorrection += (targetCorrection - m_smoothedCorrection) * SMOOTHING;

        return m_smoothedCorrection;
    }

    float GetCurrentCorrection() const { return m_smoothedCorrection; }

private:
    float m_smoothedCorrection = 0.0f;
};

} // namespace vrinject
