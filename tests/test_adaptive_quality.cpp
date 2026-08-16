#include <gtest/gtest.h>

#include "rendering/adaptive_quality_controller.h"

using namespace vrinject;

TEST(AdaptiveQualityController, StepsDownAfterSustainedOverBudgetFrames) {
    AdaptiveQualityController controller(AdaptiveQualityLevel::High);
    FrameTimingSnapshot timing{};
    timing.cpuFrameMs = 5.0;
    timing.gpuFrameMs = 14.0;
    timing.targetFrameMs = 11.111;
    timing.overBudget = true;

    controller.Update(timing);
    controller.Update(timing);
    auto settings = controller.Update(timing);

    EXPECT_EQ(settings.level, AdaptiveQualityLevel::Medium);
    EXPECT_FLOAT_EQ(settings.renderScale, 0.85f);
}

TEST(AdaptiveQualityController, EmergencyLevelRecommends2D) {
    AdaptiveQualityController controller(AdaptiveQualityLevel::Low);
    FrameTimingSnapshot timing{};
    timing.cpuFrameMs = 30.0;
    timing.gpuFrameMs = 30.0;
    timing.targetFrameMs = 11.111;
    timing.overBudget = true;

    controller.Update(timing);
    controller.Update(timing);
    auto settings = controller.Update(timing);

    EXPECT_EQ(settings.level, AdaptiveQualityLevel::Emergency);
    EXPECT_TRUE(settings.emergency2DRecommended);
}
