#pragma once
#include <chrono>
#include <cmath>
#include "core/math_types.h"

namespace vrinject {

// P1.2: Dynamic Comfort Vignette
// Monitors head angular velocity from OpenXR pose data and dynamically
// restricts peripheral FOV (via a darkened vignette) to reduce motion sickness.
// The vignette radius is proportional to angular velocity: fast motion → smaller circle → less peripheral stimulation.
class ComfortVignetteCalculator {
public:
    ComfortVignetteCalculator() = default;

    // Call once per frame with current head orientation (Euler yaw in degrees).
    // onsetDegPerSec: Angular velocity where vignette begins to appear.
    // strength: Max vignette intensity [0, 1].
    // Returns normalized vignette radius [0, 1] where 1.0 = fully open (no vignette).
    float Update(float currentYawDeg, float onsetDegPerSec, float strength) {
        auto now = std::chrono::steady_clock::now();

        if (!m_initialized) {
            m_lastYaw = currentYawDeg;
            m_lastTime = now;
            m_initialized = true;
            return 1.0f; // fully open
        }

        float dtSeconds = std::chrono::duration<float>(now - m_lastTime).count();
        if (dtSeconds < 0.0001f) return m_currentRadius;

        // Compute angular velocity in degrees per second
        float deltaYaw = fabsf(currentYawDeg - m_lastYaw);
        // Handle 360° wrap-around
        if (deltaYaw > 180.0f) deltaYaw = 360.0f - deltaYaw;
        float angularVelocity = deltaYaw / dtSeconds;

        // Target radius: fully open (1.0) when below onset, progressively smaller above
        float targetRadius = 1.0f;
        if (angularVelocity > onsetDegPerSec && onsetDegPerSec > 0.0f) {
            float excess = (angularVelocity - onsetDegPerSec) / onsetDegPerSec;
            float reduction = fminf(excess * strength, strength);
            targetRadius = fmaxf(1.0f - reduction, 0.3f); // Never fully close (min 30% FOV)
        }

        // Smooth interpolation: close fast (120ms), open slowly (400ms)
        float lerpRate = (targetRadius < m_currentRadius) ? (dtSeconds / 0.12f) : (dtSeconds / 0.40f);
        lerpRate = fminf(lerpRate, 1.0f);
        m_currentRadius = m_currentRadius + (targetRadius - m_currentRadius) * lerpRate;

        m_lastYaw = currentYawDeg;
        m_lastTime = now;
        return m_currentRadius;
    }

    float GetRadius() const { return m_currentRadius; }

private:
    float m_lastYaw = 0.0f;
    std::chrono::steady_clock::time_point m_lastTime;
    bool m_initialized = false;
    float m_currentRadius = 1.0f; // fully open
};

} // namespace vrinject
