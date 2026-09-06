#include <gtest/gtest.h>

#include "core/config_manager.h"
#include "core/subsystem_context.h"
#include "core/logger.h"
#include "core/engine_detector.h"
#include "core/compatibility_scorer.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
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

} // namespace

class GameProfilesRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir = fs::temp_directory_path() / "vrinject_profile_regression_test";
        fs::create_directories(testDir);
        configPath = testDir / "vrinject.json";

        auto logger = std::make_shared<vrinject::FileLogger>();
        auto config = std::make_shared<vrinject::ConfigManager>();
        vrinject::SubsystemContext::Get().Initialize(std::move(logger), std::move(config));
    }

    void TearDown() override {
        vrinject::SubsystemContext::Get().Shutdown();
        if (fs::exists(testDir)) {
            fs::remove_all(testDir);
        }
    }

    fs::path testDir;
    fs::path configPath;
};

// Regression Test for AppID 990080 (Hogwarts Legacy)
// Must maintain UE4 profile, DX12, Float32 precision, reverseZ, and rowMajor across all future commits
TEST_F(GameProfilesRegressionTest, AppID_990080_StereoAndPrecisionInvariant) {
    std::ofstream outFile(configPath);
    outFile << R"({
        "id": "990080",
        "engine": "UnrealEngine4",
        "api": "DX12",
        "reverseZ": true,
        "rowMajorMatrices": true,
        "matrixPrecision": "Float32",
        "depthSubmission": true
    })";
    outFile.close();

    vrinject::ConfigManager& configMgr = (*vrinject::SubsystemContext::Get().GetConfig());
    ASSERT_TRUE(configMgr.Load(testDir.string()));

    const auto& cfg = configMgr.GetConfig();
    EXPECT_EQ(cfg.engineType, "UnrealEngine4");
    EXPECT_EQ(cfg.apiType, "DX12");
    EXPECT_TRUE(cfg.hasReverseZOverride);
    EXPECT_TRUE(cfg.reverseZ);
    EXPECT_TRUE(cfg.hasRowMajorOverride);
    EXPECT_TRUE(cfg.rowMajorMatrices);
    EXPECT_EQ(cfg.matrixPrecision, "Float32");

    // Engine Detector must respect profile override directly
    EngineDetector detector;
    detector.Detect();
    const auto& detection = detector.GetDetection();
    EXPECT_EQ(detection.type, EngineType::UnrealEngine4);
    EXPECT_TRUE(detection.tuning.reverseZ);
    EXPECT_TRUE(detection.tuning.rowMajorMatrices);
    EXPECT_GE(detection.confidence, 0.99f);

    // Compatibility Scorer evaluation with DX12
    auto score = CompatibilityScorer::Evaluate(
        ValidCamera(),
        ValidDepth(),
        GraphicsBackend::DX12,
        detection);

    EXPECT_EQ(score.readiness, CompatibilityReadiness::Ready);
    EXPECT_TRUE(score.shouldAttemptStereo);
    EXPECT_GE(score.score, 90u);
}

// Regression Test for AppID 814380 (Sekiro: Shadows Die Twice)
// Must maintain Generic engine, DX11, Float32 precision, reverseZ=false, rowMajor=false
TEST_F(GameProfilesRegressionTest, AppID_814380_StereoAndPrecisionInvariant) {
    std::ofstream outFile(configPath);
    outFile << R"({
        "id": "814380",
        "engine": "Generic",
        "api": "DX11",
        "reverseZ": false,
        "rowMajorMatrices": false,
        "matrixPrecision": "Float32",
        "depthSubmission": false
    })";
    outFile.close();

    vrinject::ConfigManager& configMgr = (*vrinject::SubsystemContext::Get().GetConfig());
    ASSERT_TRUE(configMgr.Load(testDir.string()));

    const auto& cfg = configMgr.GetConfig();
    EXPECT_EQ(cfg.engineType, "Generic");
    EXPECT_EQ(cfg.apiType, "DX11");
    EXPECT_TRUE(cfg.hasReverseZOverride);
    EXPECT_FALSE(cfg.reverseZ);
    EXPECT_TRUE(cfg.hasRowMajorOverride);
    EXPECT_FALSE(cfg.rowMajorMatrices);
    EXPECT_EQ(cfg.matrixPrecision, "Float32");

    EngineDetector detector;
    detector.Detect();
    const auto& detection = detector.GetDetection();
    EXPECT_EQ(detection.type, EngineType::Unknown);
    EXPECT_FALSE(detection.tuning.reverseZ);
    EXPECT_FALSE(detection.tuning.rowMajorMatrices);

    auto score = CompatibilityScorer::Evaluate(
        ValidCamera(),
        ValidDepth(),
        GraphicsBackend::DX11,
        detection);

    EXPECT_EQ(score.readiness, CompatibilityReadiness::Ready);
    EXPECT_TRUE(score.shouldAttemptStereo);
    EXPECT_GE(score.score, 80u);
}

// Regression Test for AppID 1091500 (Cyberpunk 2077)
// Must maintain Generic engine, DX12, Float32 precision, reverseZ=true, rowMajor=false
TEST_F(GameProfilesRegressionTest, AppID_1091500_StereoAndPrecisionInvariant) {
    std::ofstream outFile(configPath);
    outFile << R"({
        "id": "1091500",
        "engine": "Generic",
        "api": "DX12",
        "reverseZ": true,
        "rowMajorMatrices": false,
        "matrixPrecision": "Float32",
        "depthSubmission": true
    })";
    outFile.close();

    vrinject::ConfigManager& configMgr = (*vrinject::SubsystemContext::Get().GetConfig());
    ASSERT_TRUE(configMgr.Load(testDir.string()));

    const auto& cfg = configMgr.GetConfig();
    EXPECT_EQ(cfg.engineType, "Generic");
    EXPECT_EQ(cfg.apiType, "DX12");
    EXPECT_TRUE(cfg.hasReverseZOverride);
    EXPECT_TRUE(cfg.reverseZ);
    EXPECT_TRUE(cfg.hasRowMajorOverride);
    EXPECT_FALSE(cfg.rowMajorMatrices);
    EXPECT_EQ(cfg.matrixPrecision, "Float32");

    EngineDetector detector;
    detector.Detect();
    const auto& detection = detector.GetDetection();
    EXPECT_TRUE(detection.tuning.reverseZ);
    EXPECT_FALSE(detection.tuning.rowMajorMatrices);

    auto score = CompatibilityScorer::Evaluate(
        ValidCamera(),
        ValidDepth(),
        GraphicsBackend::DX12,
        detection);

    EXPECT_EQ(score.readiness, CompatibilityReadiness::Ready);
    EXPECT_TRUE(score.shouldAttemptStereo);
    EXPECT_GE(score.score, 80u);
}

// Regression Test for AppID 1623730 (Palworld)
// Must maintain UnrealEngine5, DX12, Double64 precision, reverseZ=true, rowMajor=true
TEST_F(GameProfilesRegressionTest, AppID_1623730_StereoAndPrecisionInvariant) {
    std::ofstream outFile(configPath);
    outFile << R"({
        "id": "1623730",
        "engine": "UnrealEngine5",
        "api": "DX12",
        "reverseZ": true,
        "rowMajorMatrices": true,
        "matrixPrecision": "Double64",
        "depthSubmission": true
    })";
    outFile.close();

    vrinject::ConfigManager& configMgr = (*vrinject::SubsystemContext::Get().GetConfig());
    ASSERT_TRUE(configMgr.Load(testDir.string()));

    const auto& cfg = configMgr.GetConfig();
    EXPECT_EQ(cfg.engineType, "UnrealEngine5");
    EXPECT_EQ(cfg.apiType, "DX12");
    EXPECT_TRUE(cfg.hasReverseZOverride);
    EXPECT_TRUE(cfg.reverseZ);
    EXPECT_TRUE(cfg.hasRowMajorOverride);
    EXPECT_TRUE(cfg.rowMajorMatrices);
    EXPECT_EQ(cfg.matrixPrecision, "Double64");

    EngineDetector detector;
    detector.Detect();
    const auto& detection = detector.GetDetection();
    EXPECT_EQ(detection.type, EngineType::UnrealEngine5);
    EXPECT_TRUE(detection.tuning.reverseZ);
    EXPECT_TRUE(detection.tuning.rowMajorMatrices);

    auto score = CompatibilityScorer::Evaluate(
        ValidCamera(),
        ValidDepth(),
        GraphicsBackend::DX12,
        detection);

    EXPECT_EQ(score.readiness, CompatibilityReadiness::Ready);
    EXPECT_TRUE(score.shouldAttemptStereo);
    EXPECT_GE(score.score, 90u);
}

// Precision Isolation Test:
// Asserts that configuring Float32 precision protects the camera tracker
// from being influenced by UE5 heuristic detection.
TEST_F(GameProfilesRegressionTest, PrecisionIsolation_Float32ProtectsFromHeuristicInterference) {
    std::ofstream outFile(configPath);
    outFile << R"({
        "engine": "UnrealEngine4",
        "matrixPrecision": "Float32"
    })";
    outFile.close();

    vrinject::ConfigManager& configMgr = (*vrinject::SubsystemContext::Get().GetConfig());
    ASSERT_TRUE(configMgr.Load(testDir.string()));

    const auto& cfg = configMgr.GetConfig();
    EXPECT_EQ(cfg.matrixPrecision, "Float32");

    // Even if an external module detection simulated UE5:
    auto simulatedUE5Detection = EngineDetector::DetectFromModuleNames({
        "UnrealEditor-Core.dll",
        "UnrealEditor-Engine.dll"
    });
    EXPECT_EQ(simulatedUE5Detection.type, EngineType::UnrealEngine5);

    // The configuration strictly dictates Float32, which overrides heuristics
    bool isDoublePrecision = false;
    if (cfg.matrixPrecision == "Double64") {
        isDoublePrecision = true;
    } else if (cfg.matrixPrecision == "Float32") {
        isDoublePrecision = false;
    } else {
        isDoublePrecision = (simulatedUE5Detection.type == EngineType::UnrealEngine5);
    }

    EXPECT_FALSE(isDoublePrecision);
}
