#pragma once
#include <chrono>
#include <cstdint>

namespace vrinject {

class AswManager {
public:
    AswManager();

    // Evaluate if we should synthesize a frame based on the target FPS and time since last frame
    bool ShouldSynthesizeFrame(uint32_t targetFps);

    // Call when the game provides a real new frame
    void OnRealFrameSubmitted();

    // Call when a synthetic frame is submitted
    void OnSyntheticFrameSubmitted();

private:
    using clock = std::chrono::steady_clock;
    clock::time_point lastRealFrameTime_;
    clock::time_point lastSubmitTime_;
};

} // namespace vrinject
