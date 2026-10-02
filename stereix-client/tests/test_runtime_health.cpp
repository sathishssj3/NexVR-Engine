#include <gtest/gtest.h>
#include "core/runtime_state_monitor.h"

using namespace vrinject;

TEST(RuntimeHealthMonitorTest, BasicStateTransitions) {
    RuntimeStateMonitor monitor;
    monitor.BeginFrame(1);
    
    // Everything healthy
    monitor.UpdateDx11Health(true);
    monitor.UpdateCameraHealth(true);
    monitor.UpdateDepthHealth(true);
    monitor.UpdateStereoHealth(true);
    monitor.UpdateOpenXrHealth(true);
    
    auto report = monitor.EvaluateHealth();
    EXPECT_EQ(report.state, RuntimeHealthState::RUNNING);
    
    // Drop camera
    monitor.BeginFrame(2);
    monitor.UpdateCameraHealth(false);
    report = monitor.EvaluateHealth();
    EXPECT_EQ(report.state, RuntimeHealthState::DEGRADED);
    
    // Drop DX11 (fatal)
    monitor.BeginFrame(3);
    monitor.UpdateDx11Health(false);
    report = monitor.EvaluateHealth();
    EXPECT_EQ(report.state, RuntimeHealthState::FAILED);
}
