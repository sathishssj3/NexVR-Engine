// test_sdk_dx12.cpp — Functional and error-contract tests for the D3D12 SDK API

#include <gtest/gtest.h>
#include <d3d12.h>
#include <string>

#include "nexvr_sdk.h"

TEST(SdkDX12ApiTest, GetGraphicsRequirementsNullRejection) {
    NexVR_Result res = NexVR_GetGraphicsRequirementsDX12(nullptr);
    EXPECT_EQ(res, NEXVR_ERROR_INVALID_ARGUMENT);
    const char* detail = NexVR_GetLastErrorDetail();
    ASSERT_NE(detail, nullptr);
    EXPECT_STRNE(detail, "");
}

TEST(SdkDX12ApiTest, GetGraphicsRequirementsWrongStructSize) {
    NexVR_GraphicsRequirements req{};
    req.structSize = sizeof(NexVR_GraphicsRequirements) - 4;
    NexVR_Result res = NexVR_GetGraphicsRequirementsDX12(&req);
    EXPECT_EQ(res, NEXVR_ERROR_INVALID_ARGUMENT);
}

TEST(SdkDX12ApiTest, InitializeNullInfoRejection) {
    NexVR_Session session = nullptr;
    NexVR_Result res = NexVR_InitializeDX12(nullptr, &session);
    EXPECT_EQ(res, NEXVR_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(session, nullptr);
}

TEST(SdkDX12ApiTest, InitializeNullOutSessionRejection) {
    NexVR_InitializeDX12Info info{};
    info.structSize = sizeof(NexVR_InitializeDX12Info);
    info.colorSpace = NEXVR_COLOR_SPACE_SRGB_ENCODED;
    NexVR_Result res = NexVR_InitializeDX12(&info, nullptr);
    EXPECT_EQ(res, NEXVR_ERROR_INVALID_ARGUMENT);
}

TEST(SdkDX12ApiTest, InitializeWithoutRequirementsQuery) {
    NexVR_InitializeDX12Info info{};
    info.structSize = sizeof(NexVR_InitializeDX12Info);
    info.colorSpace = NEXVR_COLOR_SPACE_SRGB_ENCODED;
    NexVR_Session session = nullptr;

    NexVR_Result res = NexVR_InitializeDX12(&info, &session);
    // Must fail with requirements not queried (or runtime unavailable if uninitialized)
    EXPECT_TRUE(res == NEXVR_ERROR_REQUIREMENTS_NOT_QUERIED ||
                res == NEXVR_ERROR_RUNTIME_UNAVAILABLE ||
                res == NEXVR_ERROR_NO_HEADSET);
    EXPECT_EQ(session, nullptr);
}

TEST(SdkDX12ApiTest, InitializeWrongStructSize) {
    NexVR_InitializeDX12Info info{};
    info.structSize = sizeof(NexVR_InitializeDX12Info) - 8;
    info.colorSpace = NEXVR_COLOR_SPACE_SRGB_ENCODED;
    NexVR_Session session = nullptr;

    NexVR_Result res = NexVR_InitializeDX12(&info, &session);
    EXPECT_EQ(res, NEXVR_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(session, nullptr);
}

TEST(SdkDX12ApiTest, SubmitFrameNullSession) {
    NexVR_Result res = NexVR_SubmitFrameDX12(nullptr, 12345, nullptr, D3D12_RESOURCE_STATE_RENDER_TARGET);
    EXPECT_EQ(res, NEXVR_ERROR_INVALID_SESSION);
}

TEST(SdkDX12ApiTest, SubmitFrameWithDepthNullSession) {
    NexVR_DepthInfoDX12 depth{};
    depth.structSize = sizeof(NexVR_DepthInfoDX12);
    depth.nearZ = 0.1f;
    depth.farZ = 1000.0f;
    depth.range = NEXVR_DEPTH_RANGE_ZERO_TO_ONE;

    NexVR_Result res = NexVR_SubmitFrameWithDepthDX12(nullptr, 12345, nullptr,
                                                      D3D12_RESOURCE_STATE_RENDER_TARGET, &depth);
    EXPECT_EQ(res, NEXVR_ERROR_INVALID_SESSION);
}

TEST(SdkDX12ApiTest, ShutdownNullSafe) {
    // Calling shutdown on a null session must be a clean no-op
    EXPECT_NO_FATAL_FAILURE(NexVR_Shutdown(nullptr));
}

TEST(SdkDX12ApiTest, GetLastErrorDetailNeverNull) {
    const char* detail = NexVR_GetLastErrorDetail();
    ASSERT_NE(detail, nullptr);
}
