// test_sdk_validation.cpp — Pure unit tests for NexVR SDK validation logic (D3D11 and D3D12)

#include <gtest/gtest.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi.h>
#include <cmath>
#include <limits>
#include <string>

#include "nexvr_sdk.h"
#include "sdk_validation.h"

using namespace nexvr::sdk;

namespace {

RuntimeConstraints MakeValidConstraints() {
    RuntimeConstraints c{};
    c.adapterLuid.LowPart = 0x1234;
    c.adapterLuid.HighPart = 0;
    c.requiredWidth = 2048;
    c.requiredHeight = 1024;
    c.swapchainFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    c.queried = true;
    return c;
}

TextureFacts MakeValidTextureFacts() {
    TextureFacts t{};
    t.width = 2048;
    t.height = 1024;
    t.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    t.valid = true;
    t.sampleCount = 1;
    return t;
}

TextureFacts MakeValidDepthFacts() {
    TextureFacts t{};
    t.width = 2048;
    t.height = 1024;
    t.format = DXGI_FORMAT_D32_FLOAT;
    t.valid = true;
    t.sampleCount = 1;
    return t;
}

}  // namespace

// ===========================================================================
// DX11 Validation Tests
// ===========================================================================

TEST(SdkValidationDX11Test, RequirementsNotQueried) {
    RuntimeConstraints c = MakeValidConstraints();
    c.queried = false;
    TextureFacts t = MakeValidTextureFacts();
    std::string detail;

    NexVR_Result res = ValidateInitialize(c, t, NEXVR_COLOR_SPACE_SRGB_ENCODED,
                                          c.adapterLuid, true, true, detail);
    EXPECT_EQ(res, NEXVR_ERROR_REQUIREMENTS_NOT_QUERIED);
    EXPECT_FALSE(detail.empty());
}

TEST(SdkValidationDX11Test, AdapterMismatch) {
    RuntimeConstraints c = MakeValidConstraints();
    TextureFacts t = MakeValidTextureFacts();
    LUID wrongLuid{0x5678, 0};
    std::string detail;

    NexVR_Result res = ValidateInitialize(c, t, NEXVR_COLOR_SPACE_SRGB_ENCODED,
                                          wrongLuid, true, true, detail);
    EXPECT_EQ(res, NEXVR_ERROR_WRONG_ADAPTER);
    EXPECT_FALSE(detail.empty());
}

TEST(SdkValidationDX11Test, NotImmediateContext) {
    RuntimeConstraints c = MakeValidConstraints();
    TextureFacts t = MakeValidTextureFacts();
    std::string detail;

    NexVR_Result res = ValidateInitialize(c, t, NEXVR_COLOR_SPACE_SRGB_ENCODED,
                                          c.adapterLuid, false, true, detail);
    EXPECT_EQ(res, NEXVR_ERROR_NOT_IMMEDIATE_CONTEXT);
    EXPECT_FALSE(detail.empty());
}

TEST(SdkValidationDX11Test, MultithreadUnavailable) {
    RuntimeConstraints c = MakeValidConstraints();
    TextureFacts t = MakeValidTextureFacts();
    std::string detail;

    NexVR_Result res = ValidateInitialize(c, t, NEXVR_COLOR_SPACE_SRGB_ENCODED,
                                          c.adapterLuid, true, false, detail);
    EXPECT_EQ(res, NEXVR_ERROR_MULTITHREAD_UNAVAILABLE);
    EXPECT_FALSE(detail.empty());
}

TEST(SdkValidationDX11Test, DimensionMismatch) {
    RuntimeConstraints c = MakeValidConstraints();
    TextureFacts t = MakeValidTextureFacts();
    t.width = 1920;
    std::string detail;

    NexVR_Result res = ValidateInitialize(c, t, NEXVR_COLOR_SPACE_SRGB_ENCODED,
                                          c.adapterLuid, true, true, detail);
    EXPECT_EQ(res, NEXVR_ERROR_DIMENSION_MISMATCH);
    EXPECT_FALSE(detail.empty());
}

TEST(SdkValidationDX11Test, IncompatibleFormat) {
    RuntimeConstraints c = MakeValidConstraints();
    c.swapchainFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    TextureFacts t = MakeValidTextureFacts();
    t.format = DXGI_FORMAT_B8G8R8A8_UNORM;  // BGRA does not share parent with RGBA
    std::string detail;

    NexVR_Result res = ValidateInitialize(c, t, NEXVR_COLOR_SPACE_SRGB_ENCODED,
                                          c.adapterLuid, true, true, detail);
    EXPECT_EQ(res, NEXVR_ERROR_INVALID_FORMAT);
    EXPECT_FALSE(detail.empty());
}

TEST(SdkValidationDX11Test, ColorSpaceUnspecified) {
    RuntimeConstraints c = MakeValidConstraints();
    TextureFacts t = MakeValidTextureFacts();
    std::string detail;

    NexVR_Result res = ValidateInitialize(c, t, NEXVR_COLOR_SPACE_UNSPECIFIED,
                                          c.adapterLuid, true, true, detail);
    EXPECT_EQ(res, NEXVR_ERROR_COLOR_SPACE_UNSPECIFIED);
    EXPECT_FALSE(detail.empty());
}

TEST(SdkValidationDX11Test, ValidInitialization) {
    RuntimeConstraints c = MakeValidConstraints();
    TextureFacts t = MakeValidTextureFacts();
    std::string detail;

    NexVR_Result res = ValidateInitialize(c, t, NEXVR_COLOR_SPACE_SRGB_ENCODED,
                                          c.adapterLuid, true, true, detail);
    EXPECT_EQ(res, NEXVR_SUCCESS);
    EXPECT_TRUE(detail.empty());
}

// ===========================================================================
// DX12 Validation Tests
// ===========================================================================

TEST(SdkValidationDX12Test, RequirementsNotQueried) {
    RuntimeConstraints c = MakeValidConstraints();
    c.queried = false;
    TextureFacts t = MakeValidTextureFacts();
    std::string detail;

    NexVR_Result res = ValidateInitializeDX12(c, t, NEXVR_COLOR_SPACE_SRGB_ENCODED,
                                              c.adapterLuid, true, detail);
    EXPECT_EQ(res, NEXVR_ERROR_REQUIREMENTS_NOT_QUERIED);
    EXPECT_FALSE(detail.empty());
}

TEST(SdkValidationDX12Test, WrongAdapterLuid) {
    RuntimeConstraints c = MakeValidConstraints();
    TextureFacts t = MakeValidTextureFacts();
    LUID wrongLuid{0x9999, 0};
    std::string detail;

    NexVR_Result res = ValidateInitializeDX12(c, t, NEXVR_COLOR_SPACE_SRGB_ENCODED,
                                              wrongLuid, true, detail);
    EXPECT_EQ(res, NEXVR_ERROR_WRONG_ADAPTER);
    EXPECT_FALSE(detail.empty());
}

TEST(SdkValidationDX12Test, NonDirectCommandQueue) {
    RuntimeConstraints c = MakeValidConstraints();
    TextureFacts t = MakeValidTextureFacts();
    std::string detail;

    // queueIsDirect = false (e.g. compute or copy queue)
    NexVR_Result res = ValidateInitializeDX12(c, t, NEXVR_COLOR_SPACE_SRGB_ENCODED,
                                              c.adapterLuid, false, detail);
    EXPECT_EQ(res, NEXVR_ERROR_INVALID_COMMAND_QUEUE);
    EXPECT_FALSE(detail.empty());
}

TEST(SdkValidationDX12Test, InvalidRepresentativeTarget) {
    RuntimeConstraints c = MakeValidConstraints();
    TextureFacts t = MakeValidTextureFacts();
    t.valid = false;
    std::string detail;

    NexVR_Result res = ValidateInitializeDX12(c, t, NEXVR_COLOR_SPACE_SRGB_ENCODED,
                                              c.adapterLuid, true, detail);
    EXPECT_EQ(res, NEXVR_ERROR_INVALID_ARGUMENT);
    EXPECT_FALSE(detail.empty());
}

TEST(SdkValidationDX12Test, DimensionMismatch) {
    RuntimeConstraints c = MakeValidConstraints();
    TextureFacts t = MakeValidTextureFacts();
    t.height = 2048;  // mismatch with required 1024
    std::string detail;

    NexVR_Result res = ValidateInitializeDX12(c, t, NEXVR_COLOR_SPACE_SRGB_ENCODED,
                                              c.adapterLuid, true, detail);
    EXPECT_EQ(res, NEXVR_ERROR_DIMENSION_MISMATCH);
    EXPECT_FALSE(detail.empty());
}

TEST(SdkValidationDX12Test, IncompatibleFormat) {
    RuntimeConstraints c = MakeValidConstraints();
    c.swapchainFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    TextureFacts t = MakeValidTextureFacts();
    t.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    std::string detail;

    NexVR_Result res = ValidateInitializeDX12(c, t, NEXVR_COLOR_SPACE_SRGB_ENCODED,
                                              c.adapterLuid, true, detail);
    EXPECT_EQ(res, NEXVR_ERROR_INVALID_FORMAT);
    EXPECT_FALSE(detail.empty());
}

TEST(SdkValidationDX12Test, CompatibleFormatSharesTypelessParent) {
    RuntimeConstraints c = MakeValidConstraints();
    c.swapchainFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    TextureFacts t = MakeValidTextureFacts();
    t.format = DXGI_FORMAT_R8G8B8A8_UNORM;  // Both parent to R8G8B8A8_TYPELESS
    std::string detail;

    NexVR_Result res = ValidateInitializeDX12(c, t, NEXVR_COLOR_SPACE_SRGB_ENCODED,
                                              c.adapterLuid, true, detail);
    EXPECT_EQ(res, NEXVR_SUCCESS);
    EXPECT_TRUE(detail.empty());
}

TEST(SdkValidationDX12Test, ColorSpaceUnspecified) {
    RuntimeConstraints c = MakeValidConstraints();
    TextureFacts t = MakeValidTextureFacts();
    std::string detail;

    NexVR_Result res = ValidateInitializeDX12(c, t, NEXVR_COLOR_SPACE_UNSPECIFIED,
                                              c.adapterLuid, true, detail);
    EXPECT_EQ(res, NEXVR_ERROR_COLOR_SPACE_UNSPECIFIED);
    EXPECT_FALSE(detail.empty());
}

TEST(SdkValidationDX12Test, ValidInitialization) {
    RuntimeConstraints c = MakeValidConstraints();
    TextureFacts t = MakeValidTextureFacts();
    std::string detail;

    NexVR_Result res = ValidateInitializeDX12(c, t, NEXVR_COLOR_SPACE_LINEAR,
                                              c.adapterLuid, true, detail);
    EXPECT_EQ(res, NEXVR_SUCCESS);
    EXPECT_TRUE(detail.empty());
}

// ===========================================================================
// DX12 Depth Submission Validation Tests
// ===========================================================================

TEST(SdkValidationDepthDX12Test, StructSizeTooSmall) {
    TextureFacts color = MakeValidTextureFacts();
    TextureFacts depth = MakeValidDepthFacts();
    std::string detail;

    NexVR_Result res = ValidateDepthSubmitDX12(sizeof(NexVR_DepthInfoDX12) - 4,
                                               color, depth, 0.1f, 1000.0f, detail);
    EXPECT_EQ(res, NEXVR_ERROR_INVALID_ARGUMENT);
    EXPECT_FALSE(detail.empty());
}

TEST(SdkValidationDepthDX12Test, InvalidDepthTarget) {
    TextureFacts color = MakeValidTextureFacts();
    TextureFacts depth = MakeValidDepthFacts();
    depth.valid = false;
    std::string detail;

    NexVR_Result res = ValidateDepthSubmitDX12(sizeof(NexVR_DepthInfoDX12),
                                               color, depth, 0.1f, 1000.0f, detail);
    EXPECT_EQ(res, NEXVR_ERROR_INVALID_ARGUMENT);
    EXPECT_FALSE(detail.empty());
}

TEST(SdkValidationDepthDX12Test, NonDepthFormatRejected) {
    TextureFacts color = MakeValidTextureFacts();
    TextureFacts depth = MakeValidDepthFacts();
    depth.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    std::string detail;

    NexVR_Result res = ValidateDepthSubmitDX12(sizeof(NexVR_DepthInfoDX12),
                                               color, depth, 0.1f, 1000.0f, detail);
    EXPECT_EQ(res, NEXVR_ERROR_INVALID_FORMAT);
    EXPECT_FALSE(detail.empty());
}

TEST(SdkValidationDepthDX12Test, AcceptedDepthFormats) {
    TextureFacts color = MakeValidTextureFacts();
    std::string detail;

    const DXGI_FORMAT validFormats[] = {
        DXGI_FORMAT_D32_FLOAT,
        DXGI_FORMAT_D24_UNORM_S8_UINT,
        DXGI_FORMAT_D16_UNORM,
        DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
        DXGI_FORMAT_R32_TYPELESS,
        DXGI_FORMAT_R24G8_TYPELESS,
        DXGI_FORMAT_R16_TYPELESS,
        DXGI_FORMAT_R32G8X24_TYPELESS,
    };

    for (DXGI_FORMAT fmt : validFormats) {
        TextureFacts depth = MakeValidDepthFacts();
        depth.format = fmt;
        NexVR_Result res = ValidateDepthSubmitDX12(sizeof(NexVR_DepthInfoDX12),
                                                   color, depth, 0.1f, 1000.0f, detail);
        EXPECT_EQ(res, NEXVR_SUCCESS) << "Format " << fmt << " should be accepted";
    }
}

TEST(SdkValidationDepthDX12Test, DimensionMismatch) {
    TextureFacts color = MakeValidTextureFacts();
    TextureFacts depth = MakeValidDepthFacts();
    depth.width = 1024;  // color is 2048
    std::string detail;

    NexVR_Result res = ValidateDepthSubmitDX12(sizeof(NexVR_DepthInfoDX12),
                                               color, depth, 0.1f, 1000.0f, detail);
    EXPECT_EQ(res, NEXVR_ERROR_DIMENSION_MISMATCH);
    EXPECT_FALSE(detail.empty());
}

TEST(SdkValidationDepthDX12Test, MultisampledDepthRejected) {
    TextureFacts color = MakeValidTextureFacts();
    TextureFacts depth = MakeValidDepthFacts();
    depth.sampleCount = 4;
    std::string detail;

    NexVR_Result res = ValidateDepthSubmitDX12(sizeof(NexVR_DepthInfoDX12),
                                               color, depth, 0.1f, 1000.0f, detail);
    EXPECT_EQ(res, NEXVR_ERROR_INVALID_ARGUMENT);
    EXPECT_FALSE(detail.empty());
}

TEST(SdkValidationDepthDX12Test, SampleCountMismatch) {
    TextureFacts color = MakeValidTextureFacts();
    color.sampleCount = 2;
    TextureFacts depth = MakeValidDepthFacts();
    depth.sampleCount = 1;
    std::string detail;

    NexVR_Result res = ValidateDepthSubmitDX12(sizeof(NexVR_DepthInfoDX12),
                                               color, depth, 0.1f, 1000.0f, detail);
    EXPECT_EQ(res, NEXVR_ERROR_INVALID_ARGUMENT);
    EXPECT_FALSE(detail.empty());
}

TEST(SdkValidationDepthDX12Test, NearZNegativeOrZeroOrNanOrInf) {
    TextureFacts color = MakeValidTextureFacts();
    TextureFacts depth = MakeValidDepthFacts();
    std::string detail;

    EXPECT_EQ(ValidateDepthSubmitDX12(sizeof(NexVR_DepthInfoDX12), color, depth, 0.0f, 1000.0f, detail),
              NEXVR_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(ValidateDepthSubmitDX12(sizeof(NexVR_DepthInfoDX12), color, depth, -1.0f, 1000.0f, detail),
              NEXVR_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(ValidateDepthSubmitDX12(sizeof(NexVR_DepthInfoDX12), color, depth,
                                      std::numeric_limits<float>::infinity(), 1000.0f, detail),
              NEXVR_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(ValidateDepthSubmitDX12(sizeof(NexVR_DepthInfoDX12), color, depth,
                                      std::numeric_limits<float>::quiet_NaN(), 1000.0f, detail),
              NEXVR_ERROR_INVALID_ARGUMENT);
}

TEST(SdkValidationDepthDX12Test, FarZNegativeOrZeroOrNan) {
    TextureFacts color = MakeValidTextureFacts();
    TextureFacts depth = MakeValidDepthFacts();
    std::string detail;

    EXPECT_EQ(ValidateDepthSubmitDX12(sizeof(NexVR_DepthInfoDX12), color, depth, 0.1f, 0.0f, detail),
              NEXVR_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(ValidateDepthSubmitDX12(sizeof(NexVR_DepthInfoDX12), color, depth, 0.1f, -10.0f, detail),
              NEXVR_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(ValidateDepthSubmitDX12(sizeof(NexVR_DepthInfoDX12), color, depth, 0.1f,
                                      std::numeric_limits<float>::quiet_NaN(), detail),
              NEXVR_ERROR_INVALID_ARGUMENT);
}

TEST(SdkValidationDepthDX12Test, NearZEqualsFarZ) {
    TextureFacts color = MakeValidTextureFacts();
    TextureFacts depth = MakeValidDepthFacts();
    std::string detail;

    EXPECT_EQ(ValidateDepthSubmitDX12(sizeof(NexVR_DepthInfoDX12), color, depth, 5.0f, 5.0f, detail),
              NEXVR_ERROR_INVALID_ARGUMENT);
}

TEST(SdkValidationDepthDX12Test, StandardFrustumNearLessThanFar) {
    TextureFacts color = MakeValidTextureFacts();
    TextureFacts depth = MakeValidDepthFacts();
    std::string detail;

    EXPECT_EQ(ValidateDepthSubmitDX12(sizeof(NexVR_DepthInfoDX12), color, depth, 0.05f, 100.0f, detail),
              NEXVR_SUCCESS);
    EXPECT_TRUE(detail.empty());
}

TEST(SdkValidationDepthDX12Test, ReverseZNearGreaterThanFar) {
    TextureFacts color = MakeValidTextureFacts();
    TextureFacts depth = MakeValidDepthFacts();
    std::string detail;

    // Reverse-Z: near plane is set farther in value (or mapped inverted)
    EXPECT_EQ(ValidateDepthSubmitDX12(sizeof(NexVR_DepthInfoDX12), color, depth, 1000.0f, 0.1f, detail),
              NEXVR_SUCCESS);
    EXPECT_TRUE(detail.empty());
}

TEST(SdkValidationDepthDX12Test, InfiniteFarZ) {
    TextureFacts color = MakeValidTextureFacts();
    TextureFacts depth = MakeValidDepthFacts();
    std::string detail;

    EXPECT_EQ(ValidateDepthSubmitDX12(sizeof(NexVR_DepthInfoDX12), color, depth, 0.1f,
                                      std::numeric_limits<float>::infinity(), detail),
              NEXVR_SUCCESS);
    EXPECT_TRUE(detail.empty());
}

// ===========================================================================
// Frame State & Submit Validation
// ===========================================================================

TEST(SdkValidationFrameTest, PublishFrameStateSkippedWhenShouldRenderFalse) {
    NexVR_FrameState state{};
    state.structSize = sizeof(NexVR_FrameState);
    state.viewCount = 2;  // Poison old stack value

    NexVR_Result res = PublishFrameState(false, true, 0, nullptr, state);
    EXPECT_EQ(res, NEXVR_FRAME_SKIPPED);
    EXPECT_EQ(state.shouldRender, 0u);
    EXPECT_EQ(state.viewCount, 0u);
}

TEST(SdkValidationFrameTest, PublishFrameStateSkippedWhenPosesUnlocated) {
    NexVR_FrameState state{};
    state.structSize = sizeof(NexVR_FrameState);
    state.viewCount = 2;

    NexVR_Result res = PublishFrameState(true, false, 0, nullptr, state);
    EXPECT_EQ(res, NEXVR_FRAME_SKIPPED);
    EXPECT_EQ(state.shouldRender, 0u);
    EXPECT_EQ(state.viewCount, 0u);
}

TEST(SdkValidationFrameTest, PublishFrameStateSuccessWithPoses) {
    NexVR_FrameState state{};
    state.structSize = sizeof(NexVR_FrameState);

    NexVR_View views[2]{};
    views[0].pose.position[0] = -0.032f;
    views[1].pose.position[0] = 0.032f;

    NexVR_Result res = PublishFrameState(true, true, 2, views, state);
    EXPECT_EQ(res, NEXVR_SUCCESS);
    EXPECT_EQ(state.shouldRender, 1u);
    EXPECT_EQ(state.viewCount, 2u);
    EXPECT_FLOAT_EQ(state.views[0].pose.position[0], -0.032f);
    EXPECT_FLOAT_EQ(state.views[1].pose.position[0], 0.032f);
}

TEST(SdkValidationFrameTest, PublishFrameStateSuccessWith4Views) {
    NexVR_FrameState state{};
    state.structSize = sizeof(NexVR_FrameState);

    NexVR_View views[4]{};
    views[0].pose.position[0] = -0.032f; // Left peripheral
    views[1].pose.position[0] = 0.032f;  // Right peripheral
    views[2].pose.position[0] = -0.010f; // Left focus (fovea)
    views[3].pose.position[0] = 0.010f;  // Right focus (fovea)

    NexVR_Result res = PublishFrameState(true, true, 4, views, state);
    EXPECT_EQ(res, NEXVR_SUCCESS);
    EXPECT_EQ(state.shouldRender, 1u);
    EXPECT_EQ(state.viewCount, 4u);
    EXPECT_FLOAT_EQ(state.views[0].pose.position[0], -0.032f);
    EXPECT_FLOAT_EQ(state.views[1].pose.position[0], 0.032f);
    EXPECT_FLOAT_EQ(state.views[2].pose.position[0], -0.010f);
    EXPECT_FLOAT_EQ(state.views[3].pose.position[0], 0.010f);
}

TEST(SdkValidationFrameTest, PublishFrameStateClampsTo2ViewsForStaleStructSize) {
    NexVR_FrameState state{};
    // Simulate caller compiled against older header that only had 2 views (56 bytes smaller)
    state.structSize = sizeof(NexVR_FrameState) - 2 * sizeof(NexVR_View);

    NexVR_View views[4]{};
    views[0].pose.position[0] = -0.032f;
    views[1].pose.position[0] = 0.032f;
    views[2].pose.position[0] = -0.010f;
    views[3].pose.position[0] = 0.010f;

    NexVR_Result res = PublishFrameState(true, true, 4, views, state);
    EXPECT_EQ(res, NEXVR_SUCCESS);
    EXPECT_EQ(state.shouldRender, 1u);
    // Must be clamped to 2 to protect caller's allocated struct bounds!
    EXPECT_EQ(state.viewCount, 2u);
}

TEST(SdkValidationSubmitTest, SkippedFrameAcceptsNullTexture) {
    TextureFacts t{};
    t.valid = false;
    RuntimeConstraints c = MakeValidConstraints();
    std::string detail;

    EXPECT_EQ(ValidateSubmit(false, t, c, detail), NEXVR_SUCCESS);
}

TEST(SdkValidationSubmitTest, RenderedFrameRejectsNullTexture) {
    TextureFacts t{};
    t.valid = false;
    RuntimeConstraints c = MakeValidConstraints();
    std::string detail;

    EXPECT_EQ(ValidateSubmit(true, t, c, detail), NEXVR_ERROR_INVALID_ARGUMENT);
}

TEST(SdkValidationSubmitTest, RenderedFrameRejectsDimensionMismatch) {
    TextureFacts t = MakeValidTextureFacts();
    t.width = 1000;
    RuntimeConstraints c = MakeValidConstraints();
    std::string detail;

    EXPECT_EQ(ValidateSubmit(true, t, c, detail), NEXVR_ERROR_DIMENSION_MISMATCH);
}

// ===========================================================================
// 6DOF Controller Input & Haptics Validation Tests
// ===========================================================================

TEST(SdkValidationInputTest, ControllerStateValidatesHandIndex) {
    NexVR_ControllerState state{};
    state.structSize = sizeof(NexVR_ControllerState);
    std::string detail;

    EXPECT_EQ(ValidateControllerState(NEXVR_HAND_LEFT, &state, detail), NEXVR_SUCCESS);
    EXPECT_EQ(ValidateControllerState(NEXVR_HAND_RIGHT, &state, detail), NEXVR_SUCCESS);

    EXPECT_EQ(ValidateControllerState(static_cast<NexVR_Hand>(-1), &state, detail), NEXVR_ERROR_INVALID_ARGUMENT);
    EXPECT_FALSE(detail.empty());

    EXPECT_EQ(ValidateControllerState(static_cast<NexVR_Hand>(2), &state, detail), NEXVR_ERROR_INVALID_ARGUMENT);
    EXPECT_FALSE(detail.empty());
}

TEST(SdkValidationInputTest, ControllerStateRejectsNullState) {
    std::string detail;
    EXPECT_EQ(ValidateControllerState(NEXVR_HAND_LEFT, nullptr, detail), NEXVR_ERROR_INVALID_ARGUMENT);
    EXPECT_FALSE(detail.empty());
}

TEST(SdkValidationInputTest, ControllerStateRejectsTooSmallStructSize) {
    NexVR_ControllerState state{};
    state.structSize = sizeof(NexVR_ControllerState) - 4;
    std::string detail;

    EXPECT_EQ(ValidateControllerState(NEXVR_HAND_LEFT, &state, detail), NEXVR_ERROR_INVALID_ARGUMENT);
    EXPECT_FALSE(detail.empty());
}

TEST(SdkValidationInputTest, HapticFeedbackValidatesHandIndex) {
    NexVR_HapticFeedback haptic{};
    haptic.structSize = sizeof(NexVR_HapticFeedback);
    haptic.durationMs = 50.0f;
    haptic.frequencyHz = 160.0f;
    haptic.amplitude = 0.8f;
    std::string detail;

    EXPECT_EQ(ValidateHapticFeedback(NEXVR_HAND_LEFT, &haptic, detail), NEXVR_SUCCESS);
    EXPECT_EQ(ValidateHapticFeedback(NEXVR_HAND_RIGHT, &haptic, detail), NEXVR_SUCCESS);

    EXPECT_EQ(ValidateHapticFeedback(static_cast<NexVR_Hand>(5), &haptic, detail), NEXVR_ERROR_INVALID_ARGUMENT);
    EXPECT_FALSE(detail.empty());
}

TEST(SdkValidationInputTest, HapticFeedbackRejectsNullHaptic) {
    std::string detail;
    EXPECT_EQ(ValidateHapticFeedback(NEXVR_HAND_LEFT, nullptr, detail), NEXVR_ERROR_INVALID_ARGUMENT);
}

TEST(SdkValidationInputTest, HapticFeedbackRejectsTooSmallStructSize) {
    NexVR_HapticFeedback haptic{};
    haptic.structSize = sizeof(NexVR_HapticFeedback) - 4;
    haptic.durationMs = 50.0f;
    haptic.amplitude = 0.5f;
    std::string detail;

    EXPECT_EQ(ValidateHapticFeedback(NEXVR_HAND_LEFT, &haptic, detail), NEXVR_ERROR_INVALID_ARGUMENT);
}

TEST(SdkValidationInputTest, HapticFeedbackRejectsNegativeDuration) {
    NexVR_HapticFeedback haptic{};
    haptic.structSize = sizeof(NexVR_HapticFeedback);
    haptic.durationMs = -10.0f;
    haptic.amplitude = 0.5f;
    std::string detail;

    EXPECT_EQ(ValidateHapticFeedback(NEXVR_HAND_LEFT, &haptic, detail), NEXVR_ERROR_INVALID_ARGUMENT);
}

TEST(SdkValidationInputTest, HapticFeedbackRejectsOutOfRangeAmplitude) {
    NexVR_HapticFeedback haptic{};
    haptic.structSize = sizeof(NexVR_HapticFeedback);
    haptic.durationMs = 50.0f;
    std::string detail;

    haptic.amplitude = -0.1f;
    EXPECT_EQ(ValidateHapticFeedback(NEXVR_HAND_LEFT, &haptic, detail), NEXVR_ERROR_INVALID_ARGUMENT);

    haptic.amplitude = 1.1f;
    EXPECT_EQ(ValidateHapticFeedback(NEXVR_HAND_LEFT, &haptic, detail), NEXVR_ERROR_INVALID_ARGUMENT);
}
