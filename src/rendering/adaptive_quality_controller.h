#pragma once

#include "frame_timing_manager.h"

#include <cstdint>

namespace vrinject {

enum class AdaptiveQualityLevel {
    Ultra,
    High,
    Medium,
    Low,
    Emergency
};

struct AdaptiveQualitySettings {
    AdaptiveQualityLevel level = AdaptiveQualityLevel::High;
    float renderScale = 1.0f;
    uint32_t reconstructionRadius = 3;
    uint32_t warpQuality = 3;
    bool emergency2DRecommended = false;
};

class AdaptiveQualityController {
public:
    explicit AdaptiveQualityController(AdaptiveQualityLevel initialLevel = AdaptiveQualityLevel::High);

    AdaptiveQualitySettings Update(const FrameTimingSnapshot& timing);
    AdaptiveQualitySettings GetSettings() const { return m_settings; }
    void Reset(AdaptiveQualityLevel level = AdaptiveQualityLevel::High);

private:
    void StepDown();
    void StepUp();
    void ApplyLevel();

    AdaptiveQualitySettings m_settings{};
    uint32_t m_overBudgetFrames = 0;
    uint32_t m_headroomFrames = 0;
};

const char* ToString(AdaptiveQualityLevel level);

} // namespace vrinject
