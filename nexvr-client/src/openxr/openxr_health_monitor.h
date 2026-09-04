#pragma once

#include <cstdint>
#include <mutex>

namespace vrinject {
namespace openxr {

class OpenXRHealthMonitor {
public:
    OpenXRHealthMonitor() = default;

    void RecordFrameSubmitted(float predictedToSubmitLatencyMs) {
        std::lock_guard<std::mutex> lock(mutex_);
        framesSubmitted_++;
        
        // Simple moving average over 100 frames
        averageSubmitLatencyMs_ = (averageSubmitLatencyMs_ * 0.99f) + (predictedToSubmitLatencyMs * 0.01f);
    }

    void RecordFrameDropped() {
        std::lock_guard<std::mutex> lock(mutex_);
        framesDropped_++;
    }

    void RecordAcquireTimeout() {
        std::lock_guard<std::mutex> lock(mutex_);
        acquireTimeouts_++;
    }

    void RecordSessionLoss() {
        std::lock_guard<std::mutex> lock(mutex_);
        sessionLosses_++;
    }

    void RecordRuntimeRestart() {
        std::lock_guard<std::mutex> lock(mutex_);
        runtimeRestarts_++;
    }

    void RecordSwapchainRecreation() {
        std::lock_guard<std::mutex> lock(mutex_);
        swapchainRecreations_++;
    }

    // Getters
    uint64_t GetFramesSubmitted() const { return framesSubmitted_; }
    uint64_t GetFramesDropped() const { return framesDropped_; }
    uint32_t GetAcquireTimeouts() const { return acquireTimeouts_; }
    uint32_t GetSessionLosses() const { return sessionLosses_; }
    uint32_t GetRuntimeRestarts() const { return runtimeRestarts_; }
    uint32_t GetSwapchainRecreations() const { return swapchainRecreations_; }
    float GetAverageSubmitLatencyMs() const { return averageSubmitLatencyMs_; }

private:
    std::mutex mutex_;
    
    uint64_t framesSubmitted_ = 0;
    uint64_t framesDropped_ = 0;
    uint32_t acquireTimeouts_ = 0;
    uint32_t sessionLosses_ = 0;
    uint32_t runtimeRestarts_ = 0;
    uint32_t swapchainRecreations_ = 0;
    
    float averageSubmitLatencyMs_ = 0.0f;
};

} // namespace openxr
} // namespace vrinject
