#include "adaptive_quality_controller.h"

namespace vrinject {

AdaptiveQualityController::AdaptiveQualityController(AdaptiveQualityLevel initialLevel) {
    Reset(initialLevel);
}

AdaptiveQualitySettings AdaptiveQualityController::Update(const FrameTimingSnapshot& timing) {
    const bool overBudget = timing.overBudget || timing.droppedFrameRatio > 0.02;
    const bool hasHeadroom =
        timing.gpuFrameMs > 0.0 && timing.gpuFrameMs < (timing.targetFrameMs * 0.70) &&
        timing.cpuFrameMs < (timing.targetFrameMs * 0.85) &&
        timing.droppedFrameRatio == 0.0;

    if (overBudget) {
        m_overBudgetFrames++;
        m_headroomFrames = 0;
    } else if (hasHeadroom) {
        m_headroomFrames++;
        m_overBudgetFrames = 0;
    } else {
        m_overBudgetFrames = 0;
        m_headroomFrames = 0;
    }

    if (m_overBudgetFrames >= 3) {
        StepDown();
        m_overBudgetFrames = 0;
    } else if (m_headroomFrames >= 90) {
        StepUp();
        m_headroomFrames = 0;
    }

    return m_settings;
}

void AdaptiveQualityController::Reset(AdaptiveQualityLevel level) {
    m_settings.level = level;
    m_overBudgetFrames = 0;
    m_headroomFrames = 0;
    ApplyLevel();
}

void AdaptiveQualityController::StepDown() {
    switch (m_settings.level) {
        case AdaptiveQualityLevel::Ultra: m_settings.level = AdaptiveQualityLevel::High; break;
        case AdaptiveQualityLevel::High: m_settings.level = AdaptiveQualityLevel::Medium; break;
        case AdaptiveQualityLevel::Medium: m_settings.level = AdaptiveQualityLevel::Low; break;
        case AdaptiveQualityLevel::Low: m_settings.level = AdaptiveQualityLevel::Emergency; break;
        case AdaptiveQualityLevel::Emergency: break;
    }
    ApplyLevel();
}

void AdaptiveQualityController::StepUp() {
    switch (m_settings.level) {
        case AdaptiveQualityLevel::Emergency: m_settings.level = AdaptiveQualityLevel::Low; break;
        case AdaptiveQualityLevel::Low: m_settings.level = AdaptiveQualityLevel::Medium; break;
        case AdaptiveQualityLevel::Medium: m_settings.level = AdaptiveQualityLevel::High; break;
        case AdaptiveQualityLevel::High: m_settings.level = AdaptiveQualityLevel::Ultra; break;
        case AdaptiveQualityLevel::Ultra: break;
    }
    ApplyLevel();
}

void AdaptiveQualityController::ApplyLevel() {
    m_settings.emergency2DRecommended = false;
    switch (m_settings.level) {
        case AdaptiveQualityLevel::Ultra:
            m_settings.renderScale = 1.0f;
            m_settings.reconstructionRadius = 4;
            m_settings.warpQuality = 4;
            break;
        case AdaptiveQualityLevel::High:
            m_settings.renderScale = 1.0f;
            m_settings.reconstructionRadius = 3;
            m_settings.warpQuality = 3;
            break;
        case AdaptiveQualityLevel::Medium:
            m_settings.renderScale = 0.85f;
            m_settings.reconstructionRadius = 2;
            m_settings.warpQuality = 2;
            break;
        case AdaptiveQualityLevel::Low:
            m_settings.renderScale = 0.70f;
            m_settings.reconstructionRadius = 1;
            m_settings.warpQuality = 1;
            break;
        case AdaptiveQualityLevel::Emergency:
            m_settings.renderScale = 0.50f;
            m_settings.reconstructionRadius = 0;
            m_settings.warpQuality = 0;
            m_settings.emergency2DRecommended = true;
            break;
    }
}

const char* ToString(AdaptiveQualityLevel level) {
    switch (level) {
        case AdaptiveQualityLevel::Ultra: return "Ultra";
        case AdaptiveQualityLevel::High: return "High";
        case AdaptiveQualityLevel::Medium: return "Medium";
        case AdaptiveQualityLevel::Low: return "Low";
        case AdaptiveQualityLevel::Emergency: return "Emergency";
        default: return "Unknown";
    }
}

} // namespace vrinject
