#include "motion_predictor.h"
#include <cmath>
#include <algorithm>
#include <DirectXMath.h>

namespace vrinject {

namespace {
    inline XrQuaternionf MultiplyQuaternions(const XrQuaternionf& q1, const XrQuaternionf& q2) {
        XrQuaternionf r;
        r.w = q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z;
        r.x = q1.w * q2.x + q1.x * q2.w + q1.y * q2.z - q1.z * q2.y;
        r.y = q1.w * q2.y - q1.x * q2.z + q1.y * q2.w + q1.z * q2.x;
        r.z = q1.w * q2.z + q1.x * q2.y - q1.y * q2.x + q1.z * q2.w;
        
        // Normalize to prevent drift
        float lenSq = r.x * r.x + r.y * r.y + r.z * r.z + r.w * r.w;
        if (lenSq > 0.0f) {
            float len = std::sqrt(lenSq);
            r.x /= len;
            r.y /= len;
            r.z /= len;
            r.w /= len;
        }
        return r;
    }

    inline XrQuaternionf ConjugateQuaternion(const XrQuaternionf& q) {
        return { -q.x, -q.y, -q.z, q.w };
    }
}

MotionPredictor::MotionPredictor() {
    m_lastPose = { {0,0,0,1}, {0,0,0} };
    m_currentPose = { {0,0,0,1}, {0,0,0} };
    for (int i=0; i<3; ++i) { m_angularVelocity[i] = 0; m_linearVelocity[i] = 0; }
}

MotionPredictor::~MotionPredictor() {}

void MotionPredictor::UpdatePose(const XrPosef& newPose, std::chrono::steady_clock::time_point timestamp) {
    m_lastPose = m_currentPose;
    m_lastTime = m_currentTime;
    
    m_currentPose = newPose;
    m_currentTime = timestamp;
    
    if (m_lastTime.time_since_epoch().count() > 0) {
        float dt = std::chrono::duration<float>(m_currentTime - m_lastTime).count();
        if (dt > 0.0001f) {
            // Linear velocity
            m_linearVelocity[0] = (m_currentPose.position.x - m_lastPose.position.x) / dt;
            m_linearVelocity[1] = (m_currentPose.position.y - m_lastPose.position.y) / dt;
            m_linearVelocity[2] = (m_currentPose.position.z - m_lastPose.position.z) / dt;

            // Rotational velocity (axis-angle representation)
            XrQuaternionf deltaQ = MultiplyQuaternions(newPose.orientation, ConjugateQuaternion(m_lastPose.orientation));
            
            // Handle double cover of quaternions
            if (deltaQ.w < 0.0f) {
                deltaQ.w = -deltaQ.w;
                deltaQ.x = -deltaQ.x;
                deltaQ.y = -deltaQ.y;
                deltaQ.z = -deltaQ.z;
            }

            float w = (std::clamp)(deltaQ.w, -1.0f, 1.0f);
            float theta = 2.0f * std::acos(w);
            float sinHalfTheta = std::sqrt((std::max)(0.0f, 1.0f - w * w));
            
            if (sinHalfTheta > 0.0001f) {
                float speed = theta / dt;
                m_angularVelocity[0] = (deltaQ.x / sinHalfTheta) * speed;
                m_angularVelocity[1] = (deltaQ.y / sinHalfTheta) * speed;
                m_angularVelocity[2] = (deltaQ.z / sinHalfTheta) * speed;
            } else {
                m_angularVelocity[0] = 0.0f;
                m_angularVelocity[1] = 0.0f;
                m_angularVelocity[2] = 0.0f;
            }
        }
    }
}

XrPosef MotionPredictor::PredictPose(std::chrono::steady_clock::time_point targetTime) {
    if (m_currentTime.time_since_epoch().count() == 0) return m_currentPose;
    
    float dt = std::chrono::duration<float>(targetTime - m_currentTime).count();
    
    XrPosef predicted = m_currentPose;
    // 1. Predict position (linear dead reckoning)
    predicted.position.x += m_linearVelocity[0] * dt;
    predicted.position.y += m_linearVelocity[1] * dt;
    predicted.position.z += m_linearVelocity[2] * dt;
    
    // 2. Predict rotation (angular dead reckoning)
    float speed = std::sqrt(m_angularVelocity[0]*m_angularVelocity[0] + 
                            m_angularVelocity[1]*m_angularVelocity[1] + 
                            m_angularVelocity[2]*m_angularVelocity[2]);
    if (speed > 0.0001f) {
        float theta = speed * dt;
        float sinHalfTheta = std::sin(theta * 0.5f);
        float cosHalfTheta = std::cos(theta * 0.5f);
        
        XrQuaternionf predDelta;
        predDelta.x = (m_angularVelocity[0] / speed) * sinHalfTheta;
        predDelta.y = (m_angularVelocity[1] / speed) * sinHalfTheta;
        predDelta.z = (m_angularVelocity[2] / speed) * sinHalfTheta;
        predDelta.w = cosHalfTheta;
        
        predicted.orientation = MultiplyQuaternions(predDelta, m_currentPose.orientation);
    }
    
    return predicted;
}

AimDelta MotionPredictor::ComputeAimDelta(const XrQuaternionf& current) {
    if (!m_hasPrevious) {
        m_previousRotation = current;
        m_hasPrevious      = true;
        return { 0.0f, 0.0f };
    }

    XrQuaternionf normCurrent = current;
    float len = std::sqrt(normCurrent.x*normCurrent.x + normCurrent.y*normCurrent.y + 
                          normCurrent.z*normCurrent.z + normCurrent.w*normCurrent.w);
    if (len < 0.999f || len > 1.001f) {
        if (len > 0.0001f) {
            normCurrent.x /= len; normCurrent.y /= len; normCurrent.z /= len; normCurrent.w /= len;
        } else {
            normCurrent = {0,0,0,1};
        }
    }

    // Conjugate of previous rotation
    XrQuaternionf prevInv = {
        -m_previousRotation.x,
        -m_previousRotation.y,
        -m_previousRotation.z,
         m_previousRotation.w
    };

    // Delta quaternion = normCurrent * inverse(previous)
    XrQuaternionf delta;
    delta.x = normCurrent.w*prevInv.x + normCurrent.x*prevInv.w
            + normCurrent.y*prevInv.z - normCurrent.z*prevInv.y;
    delta.y = normCurrent.w*prevInv.y - normCurrent.x*prevInv.z
            + normCurrent.y*prevInv.w + normCurrent.z*prevInv.x;
    delta.z = normCurrent.w*prevInv.z + normCurrent.x*prevInv.y
            - normCurrent.y*prevInv.x + normCurrent.z*prevInv.w;
    delta.w = normCurrent.w*prevInv.w - normCurrent.x*prevInv.x
            - normCurrent.y*prevInv.y - normCurrent.z*prevInv.z;

    // Clamp w to valid range before acos
    float w     = std::clamp(delta.w, -1.0f, 1.0f);
    float angle = 2.0f * std::acos(std::abs(w));
    float s     = std::sqrt(1.0f - w*w);

    AimDelta result = { 0.0f, 0.0f };
    if (s > 0.001f) {
        // Extract signed pitch (X axis) and yaw (Y axis)
        result.pitchDeg = (delta.x / s) * angle
                        * (180.0f / DirectX::XM_PI)
                        * (w < 0 ? -1.0f : 1.0f);
        result.yawDeg   = (delta.y / s) * angle
                        * (180.0f / DirectX::XM_PI)
                        * (w < 0 ? -1.0f : 1.0f);
    }

    m_previousRotation = normCurrent;
    return result;
}

void MotionPredictor::Reset() {
    m_previousRotation = { 0.0f, 0.0f, 0.0f, 1.0f };
    m_hasPrevious      = false;
}

} // namespace vrinject
