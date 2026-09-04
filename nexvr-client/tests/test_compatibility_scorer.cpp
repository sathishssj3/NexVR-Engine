#include <gtest/gtest.h>

#include "core/compatibility_scorer.h"

#include <string>

using namespace vrinject;

namespace {

CameraSnapshot ValidCamera(float confidence = 0.95f) {
    CameraSnapshot camera{};
    camera.valid = true;
    camera.confidence = confidence;
    camera.resourceIdentity.nativeHandle = reinterpret_cast<void*>(0x1234);
    camera.resourceIdentity.width = 1920;
    camera.resourceIdentity.height = 1080;
    return camera;
}

DepthSnapshot ValidDepth(float confidence = 95.0f) {
    DepthSnapshot depth{};
    depth.confidence = confidence;
    depth.identity.nativeHandle = reinterpret_cast<void*>(0x5678);
    depth.identity.width = 1920;
    depth.identity.height = 1080;
    return depth;
}

EngineDetection UnrealEngine5() {
    EngineDetection engine{};
    engine.type = EngineType::UnrealEngine5;
    engine.versionString = "UE5.x";
    engine.confidence = 0.9f;
    return engine;
}

} // namespace

TEST(CompatibilityScorer, ReadyWhenCameraDepthApiAndEngineAreStrong) {
    auto score = CompatibilityScorer::Evaluate(
        ValidCamera(),
        ValidDepth(),
        GraphicsBackend::Vulkan,
        UnrealEngine5());

    EXPECT_EQ(score.readiness, CompatibilityReadiness::Ready);
    EXPECT_TRUE(score.shouldAttemptStereo);
    EXPECT_GE(score.score, 90u);
    EXPECT_EQ(score.reasonCount, 0u);
}

TEST(CompatibilityScorer, UnknownEngineIsActionableButDoesNotBlockStrongLocks) {
    EngineDetection unknown{};
    unknown.versionString = "Unknown/custom";

    auto score = CompatibilityScorer::Evaluate(
        ValidCamera(),
        ValidDepth(),
        GraphicsBackend::DX12,
        unknown);

    EXPECT_EQ(score.readiness, CompatibilityReadiness::Ready);
    EXPECT_TRUE(score.shouldAttemptStereo);
    EXPECT_GE(score.score, 70u);
    ASSERT_EQ(score.reasonCount, 1u);
    EXPECT_STREQ(score.reasons[0].signal, "engine");
}

TEST(CompatibilityScorer, BlocksStereoWhenCameraLockIsWeak) {
    auto score = CompatibilityScorer::Evaluate(
        ValidCamera(0.2f),
        ValidDepth(),
        GraphicsBackend::DX11,
        UnrealEngine5());

    EXPECT_NE(score.readiness, CompatibilityReadiness::Ready);
    EXPECT_FALSE(score.shouldAttemptStereo);
    ASSERT_GE(score.reasonCount, 1u);
    EXPECT_STREQ(score.reasons[0].signal, "camera");
}

TEST(CompatibilityScorer, BlocksStereoWhenDepthResourceIsMissing) {
    DepthSnapshot depth = ValidDepth();
    depth.identity.nativeHandle = nullptr;

    auto score = CompatibilityScorer::Evaluate(
        ValidCamera(),
        depth,
        GraphicsBackend::Vulkan,
        UnrealEngine5());

    EXPECT_FALSE(score.shouldAttemptStereo);
    bool sawDepth = false;
    for (size_t i = 0; i < score.reasonCount; ++i) {
        sawDepth = sawDepth || std::string(score.reasons[i].signal) == "depth";
    }
    EXPECT_TRUE(sawDepth);
}
