#pragma once
#include <d3d11.h>
#include <chrono>

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#include "../rendering/openxr/openxr.h"

namespace vrinject {

struct AimDelta {
    float pitchDeg;  // vertical
    float yawDeg;    // horizontal
};

class MotionPredictor {
public:
    MotionPredictor();
    ~MotionPredictor();

    void UpdatePose(const XrPosef& newPose, std::chrono::steady_clock::time_point timestamp);
    XrPosef PredictPose(std::chrono::steady_clock::time_point targetTime);

    AimDelta ComputeAimDelta(const XrQuaternionf& current);
    void     Reset();

private:
    XrPosef m_lastPose;
    XrPosef m_currentPose;
    std::chrono::steady_clock::time_point m_lastTime;
    std::chrono::steady_clock::time_point m_currentTime;
    
    // Simple velocity tracking
    float m_angularVelocity[3]; // pitch, yaw, roll rates
    float m_linearVelocity[3];  // x, y, z rates

    XrQuaternionf m_previousRotation = { 0.0f, 0.0f, 0.0f, 1.0f };
    bool          m_hasPrevious      = false;
};

} // namespace vrinject
