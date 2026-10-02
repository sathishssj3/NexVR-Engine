#include "core/runtime_state_monitor.h"

namespace vrinject {

RuntimeStateMonitor::RuntimeStateMonitor()
    : m_currentState(RuntimeHealthState::RUNNING),
      m_dx11Healthy(true),
      m_cameraHealthy(false),
      m_depthHealthy(false),
      m_stereoHealthy(false),
      m_openxrHealthy(false),
      m_currentFrame(0),
      m_droppedFrames(0),
      m_lastHealthyFrame(0) {
    m_lastReport = {};
}

void RuntimeStateMonitor::BeginFrame(uint64_t frameNumber) {
    m_currentFrame = frameNumber;
}

void RuntimeStateMonitor::UpdateDx11Health(bool healthy) {
    m_dx11Healthy = healthy;
}

void RuntimeStateMonitor::UpdateCameraHealth(bool healthy) {
    m_cameraHealthy = healthy;
}

void RuntimeStateMonitor::UpdateDepthHealth(bool healthy) {
    m_depthHealthy = healthy;
}

void RuntimeStateMonitor::UpdateStereoHealth(bool healthy) {
    m_stereoHealthy = healthy;
}

void RuntimeStateMonitor::UpdateOpenXrHealth(bool healthy) {
    m_openxrHealthy = healthy;
}

void RuntimeStateMonitor::MarkFrameDropped() {
    m_droppedFrames++;
}

RuntimeHealthReport RuntimeStateMonitor::EvaluateHealth() {
    bool fullyHealthy = m_dx11Healthy && m_cameraHealthy && m_depthHealthy && m_stereoHealthy && m_openxrHealthy;
    
    if (fullyHealthy) {
        if (m_currentState == RuntimeHealthState::DEGRADED || m_currentState == RuntimeHealthState::RECOVERING) {
            if (m_currentFrame - m_lastHealthyFrame > 3) {
                m_currentState = RuntimeHealthState::RUNNING; // Stabilized
            } else {
                m_currentState = RuntimeHealthState::RECOVERING;
            }
        } else {
            m_currentState = RuntimeHealthState::RUNNING;
        }
        m_lastHealthyFrame = m_currentFrame;
    } else {
        if (!m_dx11Healthy) {
            m_currentState = RuntimeHealthState::FAILED;
        } else {
            m_currentState = RuntimeHealthState::DEGRADED;
        }
    }

    m_lastReport = {
        m_currentState,
        m_dx11Healthy,
        m_cameraHealthy,
        m_depthHealthy,
        m_stereoHealthy,
        m_openxrHealthy,
        m_currentFrame,
        m_droppedFrames
    };

    return m_lastReport;
}

RuntimeHealthReport RuntimeStateMonitor::GetLastReport() const {
    return m_lastReport;
}

} // namespace vrinject
