// test_sdk_input.cpp — Functional and error-contract tests for 6DOF Controller Input & Haptics C API

#include <gtest/gtest.h>
#include <string>

#include "nexvr_sdk.h"

TEST(SdkInputApiTest, SyncInputRejectsNullSession) {
    NexVR_Result res = NexVR_SyncInput(nullptr);
    EXPECT_EQ(res, NEXVR_ERROR_INVALID_SESSION);
    const char* detail = NexVR_GetLastErrorDetail();
    ASSERT_NE(detail, nullptr);
    EXPECT_STRNE(detail, "");
}

TEST(SdkInputApiTest, GetControllerStateRejectsNullSession) {
    NexVR_ControllerState state{};
    state.structSize = sizeof(NexVR_ControllerState);

    NexVR_Result res = NexVR_GetControllerState(nullptr, NEXVR_HAND_LEFT, &state);
    EXPECT_EQ(res, NEXVR_ERROR_INVALID_SESSION);
    const char* detail = NexVR_GetLastErrorDetail();
    ASSERT_NE(detail, nullptr);
    EXPECT_STRNE(detail, "");
}

TEST(SdkInputApiTest, TriggerHapticRejectsNullSession) {
    NexVR_HapticFeedback haptic{};
    haptic.structSize = sizeof(NexVR_HapticFeedback);
    haptic.durationMs = 50.0f;
    haptic.amplitude = 0.5f;

    NexVR_Result res = NexVR_TriggerHaptic(nullptr, NEXVR_HAND_LEFT, &haptic);
    EXPECT_EQ(res, NEXVR_ERROR_INVALID_SESSION);
    const char* detail = NexVR_GetLastErrorDetail();
    ASSERT_NE(detail, nullptr);
    EXPECT_STRNE(detail, "");
}

TEST(SdkInputApiTest, StopHapticRejectsNullSession) {
    NexVR_Result res = NexVR_StopHaptic(nullptr, NEXVR_HAND_LEFT);
    EXPECT_EQ(res, NEXVR_ERROR_INVALID_SESSION);
    const char* detail = NexVR_GetLastErrorDetail();
    ASSERT_NE(detail, nullptr);
    EXPECT_STRNE(detail, "");
}

TEST(SdkInputApiTest, ControllerStateRejectsTooSmallStructSize) {
    NexVR_ControllerState state{};
    state.structSize = sizeof(NexVR_ControllerState) - 4;

    NexVR_Result res = NexVR_GetControllerState(nullptr, NEXVR_HAND_LEFT, &state);
    EXPECT_EQ(res, NEXVR_ERROR_INVALID_SESSION);
}

TEST(SdkInputApiTest, HapticFeedbackRejectsNegativeDuration) {
    NexVR_HapticFeedback haptic{};
    haptic.structSize = sizeof(NexVR_HapticFeedback);
    haptic.durationMs = -50.0f;
    haptic.amplitude = 0.5f;

    NexVR_Result res = NexVR_TriggerHaptic(nullptr, NEXVR_HAND_LEFT, &haptic);
    EXPECT_EQ(res, NEXVR_ERROR_INVALID_SESSION);
}

TEST(SdkInputApiTest, StopHapticNullSafeDetail) {
    (void)NexVR_StopHaptic(nullptr, NEXVR_HAND_LEFT);
    const char* detail = NexVR_GetLastErrorDetail();
    ASSERT_NE(detail, nullptr);
    EXPECT_STRNE(detail, "");
}
