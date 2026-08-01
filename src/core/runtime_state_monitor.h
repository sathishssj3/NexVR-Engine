#pragma once

#include <cstdint>

namespace vrinject {

enum class RuntimeHealthState {
    RUNNING,
    DEGRADED,
    RECOVERING,
    FAILED
};

struct RuntimeHealthReport {
    RuntimeHealthState state;

    bool dx11Healthy;
    bool cameraHealthy;
    bool depthHealthy;
    bool stereoHealthy;
    bool openxrHealthy;

    uint64_t frameNumber;
    uint64_t droppedFrames;
};

class RuntimeStateMonitor {
public:
    RuntimeStateMonitor();

    void BeginFrame(uint64_t frameNumber);
    void UpdateDx11Health(bool healthy);
    void UpdateCameraHealth(bool healthy);
    void UpdateDepthHealth(bool healthy);
    void UpdateStereoHealth(bool healthy);
    void UpdateOpenXrHealth(bool healthy);
    void MarkFrameDropped();

    RuntimeHealthReport EvaluateHealth();
    RuntimeHealthReport GetLastReport() const;

private:
    RuntimeHealthState m_currentState;
    RuntimeHealthReport m_lastReport;

    bool m_dx11Healthy;
    bool m_cameraHealthy;
    bool m_depthHealthy;
    bool m_stereoHealthy;
    bool m_openxrHealthy;

    uint64_t m_currentFrame;
    uint64_t m_droppedFrames;
    uint64_t m_lastHealthyFrame;
};

} // namespace vrinject
