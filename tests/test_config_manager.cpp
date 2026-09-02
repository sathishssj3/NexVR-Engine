#include "gtest/gtest.h"
#include "core/config_manager.h"
#include "core/subsystem_context.h"
#include "core/logger.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class ConfigManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary directory for testing
        testDir = fs::temp_directory_path() / "vrinject_config_test";
        fs::create_directories(testDir);
        configPath = testDir / "vrinject.json";
        
        auto logger = std::make_shared<vrinject::FileLogger>();
        auto config = std::make_shared<vrinject::ConfigManager>();
        vrinject::SubsystemContext::Get().Initialize(std::move(logger), std::move(config));
    }

    void TearDown() override {
        vrinject::SubsystemContext::Get().Shutdown();
        // Clean up the test directory
        if (fs::exists(testDir)) {
            fs::remove_all(testDir);
        }
    }

    fs::path testDir;
    fs::path configPath;
};

TEST_F(ConfigManagerTest, ConfigLoadDefaultWhenMissing) {
    vrinject::ConfigManager& config = (*vrinject::SubsystemContext::Get().GetConfig());

    // Load from directory where config doesn't exist
    bool result = config.Load(testDir.string());

    // Should create default config and return true
    EXPECT_TRUE(result);

    // Check that default values are set
    const vrinject::VRConfig& cfg = config.GetConfig();
    EXPECT_FLOAT_EQ(cfg.ipd, 0.064f);
    EXPECT_FLOAT_EQ(cfg.convergence, 10.0f);
    EXPECT_TRUE(cfg.enableNeuralInpainter);
    EXPECT_TRUE(cfg.enableImGuiOverlay);
    EXPECT_FLOAT_EQ(cfg.motionAimSensitivity, 1.0f);
}

TEST_F(ConfigManagerTest, ConfigLoadFromFile) {
    // Create a test config file
    std::ofstream outFile(configPath);
    outFile << R"({
        "ipd": 0.07,
        "convergence": 15.0,
        "resolutionScale": 1.5,
        "enableNeuralInpainter": false,
        "enableImGuiOverlay": false,
        "motionAimSensitivity": 2.5,
        "useRecommendedResolution": false,
        "srgbCorrection": false,
        "depthSubmission": true,
        "rawInputMode": false,
        "autoInjectOnLaunch": true,
        "vrScaleFactor": 50.0,
        "vrThreadPriority": 15,
        "shaderDir": "custom_shaders",
        "modelDir": "custom_models",
        "depthBufferMaxSizeMultiplier": 8.0
    })";
    outFile.close();

    vrinject::ConfigManager& config = (*vrinject::SubsystemContext::Get().GetConfig());
    bool result = config.Load(testDir.string());

    EXPECT_TRUE(result);

    const vrinject::VRConfig& cfg = config.GetConfig();
    EXPECT_FLOAT_EQ(cfg.ipd, 0.07f);
    EXPECT_FLOAT_EQ(cfg.convergence, 15.0f);
    EXPECT_FLOAT_EQ(cfg.resolutionScale, 1.5f);
    EXPECT_FALSE(cfg.enableNeuralInpainter);
    EXPECT_FALSE(cfg.enableImGuiOverlay);
    EXPECT_FLOAT_EQ(cfg.motionAimSensitivity, 2.5f);
    EXPECT_FALSE(cfg.useRecommendedResolution);
    EXPECT_FALSE(cfg.srgbCorrection);
    EXPECT_TRUE(cfg.depthSubmission);
    EXPECT_FALSE(cfg.rawInputMode);
    EXPECT_TRUE(cfg.autoInjectOnLaunch);
    EXPECT_FLOAT_EQ(cfg.vrScaleFactor, 50.0f);
    EXPECT_EQ(cfg.vrThreadPriority, 15);
    EXPECT_EQ(cfg.shaderDir, "custom_shaders");
    EXPECT_EQ(cfg.modelDir, "custom_models");
    EXPECT_FLOAT_EQ(cfg.depthBufferMaxSizeMultiplier, 8.0f);
}

TEST_F(ConfigManagerTest, ConfigSaveAndLoad) {
    vrinject::ConfigManager& config = (*vrinject::SubsystemContext::Get().GetConfig());
    config.Load(testDir.string());

    // Modify some settings
    vrinject::VRConfig& cfg = config.GetConfigMutable();
    cfg.ipd = 0.08f;
    cfg.convergence = 20.0f;
    cfg.enableNeuralInpainter = false;
    cfg.motionAimSensitivity = 5.0f;

    // Save the config
    bool saveResult = config.Save();
    EXPECT_TRUE(saveResult);
    EXPECT_TRUE(fs::exists(configPath));

    // Create a new config manager instance and load
    vrinject::ConfigManager& config2 = (*vrinject::SubsystemContext::Get().GetConfig());
    bool loadResult = config2.Load(testDir.string());
    EXPECT_TRUE(loadResult);

    const vrinject::VRConfig& loadedCfg = config2.GetConfig();
    EXPECT_FLOAT_EQ(loadedCfg.ipd, 0.08f);
    EXPECT_FLOAT_EQ(loadedCfg.convergence, 20.0f);
    EXPECT_FALSE(loadedCfg.enableNeuralInpainter);
    EXPECT_FLOAT_EQ(loadedCfg.motionAimSensitivity, 5.0f);
}

TEST_F(ConfigManagerTest, ConfigMotionAimSensitivityClamping) {
    // Test that values outside 0.1-10.0 range are clamped
    std::ofstream outFile(configPath);
    outFile << R"({"motionAimSensitivity": 15.0})";  // Way above max
    outFile.close();

    vrinject::ConfigManager& config = (*vrinject::SubsystemContext::Get().GetConfig());
    config.Load(testDir.string());

    const vrinject::VRConfig& cfg = config.GetConfig();
    EXPECT_FLOAT_EQ(cfg.motionAimSensitivity, 10.0f);  // Should be clamped to max

    // Test minimum clamping
    outFile.open(configPath);
    outFile << R"({"motionAimSensitivity": 0.01})";  // Way below min
    outFile.close();

    config.Load(testDir.string());
    EXPECT_FLOAT_EQ(config.GetConfig().motionAimSensitivity, 0.1f);  // Should be clamped to min
}

TEST_F(ConfigManagerTest, ConfigJsonParseErrorHandling) {
    // Create invalid JSON
    std::ofstream outFile(configPath);
    outFile << R"({"ipd": "not a number"})";
    outFile.close();

    vrinject::ConfigManager& config = (*vrinject::SubsystemContext::Get().GetConfig());
    bool result = config.Load(testDir.string());

    // Should return false but reset to defaults
    EXPECT_FALSE(result);

    const vrinject::VRConfig& cfg = config.GetConfig();
    EXPECT_FLOAT_EQ(cfg.ipd, 0.064f);  // Default value
    EXPECT_TRUE(cfg.enableNeuralInpainter);  // Default value
}

TEST_F(ConfigManagerTest, HierarchicalParentDirectoryDiscovery) {
    // Simulate game installed at testDir with binary inside Phoenix/Binaries/Win64
    fs::path subfolder = testDir / "Phoenix" / "Binaries" / "Win64";
    fs::create_directories(subfolder);

    // Place vrinject.json at root install folder testDir
    std::ofstream outFile(testDir / "vrinject.json");
    outFile << R"({
        "ipd": 0.075,
        "convergence": 18.0
    })";
    outFile.close();

    // DLL executes in subfolder and calls Load with subfolder path
    vrinject::ConfigManager& config = (*vrinject::SubsystemContext::Get().GetConfig());
    bool result = config.Load(subfolder.string());

    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(config.GetConfig().ipd, 0.075f);
    EXPECT_FLOAT_EQ(config.GetConfig().convergence, 18.0f);
}

TEST_F(ConfigManagerTest, PerGameEngineProfileOverrides) {
    std::ofstream outFile(configPath);
    outFile << R"({
        "engine": "UnrealEngine5",
        "reverseZ": false,
        "rowMajorMatrices": false
    })";
    outFile.close();

    vrinject::ConfigManager& config = (*vrinject::SubsystemContext::Get().GetConfig());
    bool result = config.Load(testDir.string());

    EXPECT_TRUE(result);
    const auto& cfg = config.GetConfig();
    EXPECT_EQ(cfg.engineType, "UnrealEngine5");
    EXPECT_TRUE(cfg.hasReverseZOverride);
    EXPECT_FALSE(cfg.reverseZ);
    EXPECT_TRUE(cfg.hasRowMajorOverride);
    EXPECT_FALSE(cfg.rowMajorMatrices);
}
