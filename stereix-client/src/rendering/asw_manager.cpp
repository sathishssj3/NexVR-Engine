#include "rendering/asw_manager.h"

namespace vrinject {

AswManager::AswManager() {
    lastRealFrameTime_ = clock::now();
    lastSubmitTime_ = clock::now();
}

bool AswManager::ShouldSynthesizeFrame(uint32_t targetFps) {
    if (targetFps == 0) return false;

    auto now = clock::now();
    double targetFrameTimeMs = 1000.0 / targetFps;
    
    // If it's been longer than the target frame time since we last submitted ANY frame, we need a frame
    auto msSinceLastSubmit = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(now - lastSubmitTime_).count();
    
    // Check if the game is falling behind the target frame rate
    auto msSinceLastReal = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(now - lastRealFrameTime_).count();
    
    // Allow a tiny margin of error (e.g. 1ms)
    if (msSinceLastSubmit >= (targetFrameTimeMs - 1.0) && msSinceLastReal > targetFrameTimeMs) {
        return true;
    }
    
    return false;
}

void AswManager::OnRealFrameSubmitted() {
    auto now = clock::now();
    lastRealFrameTime_ = now;
    lastSubmitTime_ = now;
}

void AswManager::OnSyntheticFrameSubmitted() {
    lastSubmitTime_ = clock::now();
}

} // namespace vrinject
