#include <gtest/gtest.h>

#include "core/engine_detector.h"

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

TEST(EngineDetector, MultiGameIsolation_HogwartsLegacyAndModernUE5) {
    // Hogwarts Legacy (UE4 title with PhysX / bink / win64 shipping)
    auto hlDetection = EngineDetector::DetectFromModuleNames({
        "HogwartsLegacy.exe",
        "PhysX3_x64.dll",
        "bink2w64.dll",
    });

    EXPECT_EQ(hlDetection.type, EngineType::UnrealEngine4);
    EXPECT_EQ(hlDetection.versionString, "UE4.xx");
    EXPECT_TRUE(hlDetection.tuning.reverseZ);
    EXPECT_TRUE(hlDetection.tuning.rowMajorMatrices);

    // Modern UE5 Title (e.g., MECCHA CHAMELEON / PenguinHotel / modern shipping title)
    auto ue5Detection = EngineDetector::DetectFromModuleNames({
        "Chameleon-Win64-Shipping.exe",
        "UnrealEngine5.dll",
        "GameModule.dll",
    });

    EXPECT_EQ(ue5Detection.type, EngineType::UnrealEngine5);
    EXPECT_EQ(ue5Detection.versionString, "UE5.x");
    EXPECT_TRUE(ue5Detection.tuning.reverseZ);
    EXPECT_TRUE(ue5Detection.tuning.rowMajorMatrices);

    // Verify neither detection mutually interferes with the other
    EXPECT_NE(hlDetection.type, ue5Detection.type);
    EXPECT_EQ(hlDetection.versionString, "UE4.xx");
    EXPECT_EQ(ue5Detection.versionString, "UE5.x");
}
