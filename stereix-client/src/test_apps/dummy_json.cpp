#include "core/sprint_compatibility_logger.h"
#include "core/subsystem_context.h"
#include <iostream>
int main() {
    vrinject::SubsystemContext::Get().Initialize(nullptr, nullptr);
    vrinject::SubsystemContext::Get().GetSprintCompatibilityLogger()->Initialize("sprint59_test.json");
    vrinject::SprintCompatibilityScore score;
    score.Title = "Synthetic Test App";
    score.Engine = "TrivialEngine (Mock)";
    score.EngineDetectionConfidence = 0.99f;
    score.InjectionSuccess = true;
    score.CameraLockSuccess = true;
    score.CameraTimeToLockMs = 12.5;
    score.DepthLockSuccess = true;
    score.DepthTimeToLockMs = 45.2;
    score.PredictedDisplayTimingDeltaMs = 11.1;
    score.ActualGpuTimeMs = 0.8;
    score.ActualCpuTimeMs = 0.15;
    score.FailureReason = "None";

    vrinject::SubsystemContext::Get().GetSprintCompatibilityLogger()->LogScore(score);
    vrinject::SubsystemContext::Get().GetSprintCompatibilityLogger()->Shutdown();
    std::cout << "JSON Log written." << std::endl;
    return 0;
}
