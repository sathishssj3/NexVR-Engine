#pragma once
#include "core/math_types.h"
#include <chrono>
#include <cmath>

namespace vrinject {

// P1.1: Cutscene Auto-Theater Mode
// Detects cinematic camera cuts by tracking inter-frame rotational velocity
// (angular jerk). When a cut exceeds the threshold, we smoothly blend to a
// flat "cinema screen" projection to prevent violent view snaps in VR.
class CutsceneDetector {
public:
    CutsceneDetector() = default;

    // Call once per frame with the current camera forward vector.
    // Returns a smooth weight [0, 1] where 0 = normal stereo, 1 = full theater.
    float Update(const Vector3& cameraForward, float cutThresholdDegPerSec) {
        auto now = std::chrono::steady_clock::now();

        if (!m_hasLastFrame) {
            m_lastForward = Normalize(cameraForward);
            m_lastTime = now;
            m_hasLastFrame = true;
            return 0.0f;
        }

        float dtSeconds = std::chrono::duration<float>(now - m_lastTime).count();
        if (dtSeconds < 0.0001f) return m_currentWeight;

        Vector3 currentFwd = Normalize(cameraForward);

        // Dot product clamped to prevent NaN from acos
        float dot = currentFwd.x * m_lastForward.x +
                    currentFwd.y * m_lastForward.y +
                    currentFwd.z * m_lastForward.z;
        dot = (dot > 1.0f) ? 1.0f : (dot < -1.0f) ? -1.0f : dot;

        float angleDeg = acosf(dot) * (180.0f / 3.14159265f);
        float angularVelocity = angleDeg / dtSeconds;

        // Detect cut: angular velocity spike above threshold
        if (angularVelocity > cutThresholdDegPerSec) {
            m_cutDetectedTime = now;
            m_inCutscene = true;
        }

        // Smooth blend in/out over 0.4s
        const float BLEND_DURATION = 0.4f;
        if (m_inCutscene) {
            m_currentWeight += dtSeconds / BLEND_DURATION;
            if (m_currentWeight >= 1.0f) m_currentWeight = 1.0f;

            // Exit theater mode after 2 seconds of stable camera (no cuts)
            float timeSinceCut = std::chrono::duration<float>(now - m_cutDetectedTime).count();
            if (timeSinceCut > 2.0f && angularVelocity < cutThresholdDegPerSec * 0.3f) {
                m_inCutscene = false;
            }
        } else {
            m_currentWeight -= dtSeconds / BLEND_DURATION;
            if (m_currentWeight <= 0.0f) m_currentWeight = 0.0f;
        }

        m_lastForward = currentFwd;
        m_lastTime = now;
        return m_currentWeight;
    }

    bool IsInCutscene() const { return m_inCutscene; }
    float GetWeight() const { return m_currentWeight; }

private:
    static Vector3 Normalize(const Vector3& v) {
        float len = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
        if (len < 0.0001f) return {0.0f, 0.0f, 1.0f};
        return {v.x / len, v.y / len, v.z / len};
    }

    Vector3 m_lastForward = {0.0f, 0.0f, 1.0f};
    std::chrono::steady_clock::time_point m_lastTime;
    std::chrono::steady_clock::time_point m_cutDetectedTime;
    bool m_hasLastFrame = false;
    bool m_inCutscene = false;
    float m_currentWeight = 0.0f;
};

} // namespace vrinject
