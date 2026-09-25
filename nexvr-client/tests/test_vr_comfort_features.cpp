#include <gtest/gtest.h>
#include "vr/cutscene_detector.h"
#include "vr/comfort_vignette.h"
#include "vr/horizon_lock.h"
#include "hooks/input_hook.h"
#include <thread>
#include <chrono>

using namespace vrinject;

// ============================================================================
// P1.1 Cutscene Auto-Theater Detection Tests
// ============================================================================
TEST(VRComfortFeaturesTest, CutsceneDetector_InitialState) {
    CutsceneDetector detector;
    EXPECT_FALSE(detector.IsInCutscene());
    EXPECT_FLOAT_EQ(detector.GetWeight(), 0.0f);

    Vector3 initialFwd = {0.0f, 0.0f, 1.0f};
    float weight = detector.Update(initialFwd, 90.0f);
    EXPECT_FLOAT_EQ(weight, 0.0f);
    EXPECT_FALSE(detector.IsInCutscene());
}

TEST(VRComfortFeaturesTest, CutsceneDetector_TriggersOnAngularSpike) {
    CutsceneDetector detector;
    Vector3 fwd1 = {0.0f, 0.0f, 1.0f};
    detector.Update(fwd1, 90.0f);

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // Sudden 90 degree camera cut in 20ms (> 4000 deg/sec >> 90 threshold)
    Vector3 fwd2 = {1.0f, 0.0f, 0.0f};
    float weight = detector.Update(fwd2, 90.0f);

    EXPECT_TRUE(detector.IsInCutscene());
    EXPECT_GT(weight, 0.0f);
}

TEST(VRComfortFeaturesTest, CutsceneDetector_IgnoresGentlePan) {
    CutsceneDetector detector;
    Vector3 fwd1 = {0.0f, 0.0f, 1.0f};
    detector.Update(fwd1, 90.0f);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Tiny 0.01 rad change over 50ms (~11 deg/sec < 90 threshold)
    Vector3 fwd2 = {0.01f, 0.0f, 0.9999f};
    float weight = detector.Update(fwd2, 90.0f);

    EXPECT_FALSE(detector.IsInCutscene());
    EXPECT_FLOAT_EQ(weight, 0.0f);
}

// ============================================================================
// P1.2 Dynamic Comfort Vignette Tests
// ============================================================================
TEST(VRComfortFeaturesTest, ComfortVignette_InitialState) {
    ComfortVignetteCalculator vignette;
    EXPECT_FLOAT_EQ(vignette.GetRadius(), 1.0f);

    float radius = vignette.Update(0.0f, 45.0f, 0.7f);
    EXPECT_FLOAT_EQ(radius, 1.0f);
}

TEST(VRComfortFeaturesTest, ComfortVignette_RestrictsFOVOnFastTurn) {
    ComfortVignetteCalculator vignette;
    vignette.Update(0.0f, 45.0f, 0.7f);

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // 20 degrees in 20ms = 1000 deg/sec >> 45 onset
    float radius = vignette.Update(20.0f, 45.0f, 0.7f);

    // Vignette radius should contract (< 1.0)
    EXPECT_LT(radius, 1.0f);
    EXPECT_GE(radius, 0.3f); // Must obey safe minimum FOV floor
}

TEST(VRComfortFeaturesTest, ComfortVignette_WrapAroundHandling) {
    ComfortVignetteCalculator vignette;
    vignette.Update(359.0f, 45.0f, 0.7f);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Yaw crosses 360 -> 1.0 degree: actual delta is 2.0 degrees, not 358 degrees
    // 2 deg in 50ms = 40 deg/sec, which is below 45 onset!
    float radius = vignette.Update(1.0f, 45.0f, 0.7f);

    EXPECT_FLOAT_EQ(radius, 1.0f);
}

// ============================================================================
// P2 Horizon Lock Tests
// ============================================================================
TEST(VRComfortFeaturesTest, HorizonLock_ZeroStrengthDoesNothing) {
    HorizonLock lock;
    Matrix4x4 tiltedView = {};
    // 45 degree roll in view matrix
    float cos45 = 0.7071f;
    float sin45 = 0.7071f;
    tiltedView.m[0][0] = cos45;
    tiltedView.m[0][1] = sin45;

    float corr = lock.ComputeRollCorrection(tiltedView, 0.0f);
    EXPECT_FLOAT_EQ(corr, 0.0f);
}

TEST(VRComfortFeaturesTest, HorizonLock_CorrectsRoll) {
    HorizonLock lock;
    Matrix4x4 tiltedView = {};
    // 30 degree tilt: right vector = (cos30, sin30)
    float cos30 = 0.866025f;
    float sin30 = 0.5f;
    tiltedView.m[0][0] = cos30;
    tiltedView.m[0][1] = sin30;

    // Apply multiple frames to allow exponential smoothing to converge
    float corr = 0.0f;
    for (int i = 0; i < 30; ++i) {
        corr = lock.ComputeRollCorrection(tiltedView, 1.0f);
    }

    // Expected target correction is -rollRad = -atan2(0.5, 0.866025) ≈ -0.5236 rad
    EXPECT_LT(corr, 0.0f);
    EXPECT_NEAR(corr, -0.5236f, 0.05f);
}

// ============================================================================
// P0.1 Input Hook Vibration Feedback Test
// ============================================================================
TEST(VRComfortFeaturesTest, InputHook_VibrationFeedback) {
    auto& input = InputHook::GetInstance();
    input.SetVibration(0.75f, 0.50f);

    float left = 0.0f;
    float right = 0.0f;
    bool hasVib = input.ConsumeVibration(left, right);

    EXPECT_TRUE(hasVib);
    EXPECT_FLOAT_EQ(left, 0.75f);
    EXPECT_FLOAT_EQ(right, 0.50f);

    // Second consume should return false (already consumed)
    bool secondConsume = input.ConsumeVibration(left, right);
    EXPECT_FALSE(secondConsume);
}

