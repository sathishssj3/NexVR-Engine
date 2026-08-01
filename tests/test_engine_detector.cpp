#include <gtest/gtest.h>

#include "src/core/engine_detector.h"

using namespace vrinject;

TEST(EngineDetector, DetectsUnityIl2cppFromModuleNames) {
    auto detection = EngineDetector::DetectFromModuleNames({
        "GameAssembly.dll",
        "UnityPlayer.dll",
        "SomeGame.exe",
    });

    EXPECT_EQ(detection.type, EngineType::Unity);
    EXPECT_EQ(detection.versionString, "Unity (IL2CPP)");
    EXPECT_GE(detection.confidence, 0.9f);
    EXPECT_TRUE(detection.tuning.reverseZ);
}

TEST(EngineDetector, DetectsUnreal5FromModuleNames) {
    auto detection = EngineDetector::DetectFromModuleNames({
        "UnrealEditor-Core.dll",
        "UnrealEditor-Engine.dll",
        "GameModule.dll",
    });

    EXPECT_EQ(detection.type, EngineType::UnrealEngine5);
    EXPECT_EQ(detection.versionString, "UE5.x");
    EXPECT_GE(detection.confidence, 0.9f);
    EXPECT_TRUE(detection.tuning.rowMajorMatrices);
}

TEST(EngineDetector, DetectsUnreal4FromUE4ModuleNames) {
    auto detection = EngineDetector::DetectFromModuleNames({
        "UE4Game-Win64-Shipping.exe",
        "PhysX3_x64.dll",
    });

    EXPECT_EQ(detection.type, EngineType::UnrealEngine4);
    EXPECT_EQ(detection.versionString, "UE4.xx");
    EXPECT_GE(detection.confidence, 0.9f);
}

TEST(EngineDetector, UnknownWhenNoEngineSignalsExist) {
    auto detection = EngineDetector::DetectFromModuleNames({
        "custom_renderer.dll",
        "game.exe",
    });

    EXPECT_EQ(detection.type, EngineType::Unknown);
    EXPECT_EQ(detection.versionString, "Unknown/custom");
    EXPECT_EQ(detection.confidence, 0.0f);
    EXPECT_TRUE(detection.tuning.excludeShadowPasses);
}
