#pragma once

#include <string>
#include <windows.h>

namespace vrinject {

struct VRConfig {
    VRConfig() = default;
    
    float ipd = 0.064f; // Meters
    float convergence = 10.0f; // Focal plane distance
    float resolutionScale = 1.0f;
    bool enableNeuralInpainter = true;
    bool enableImGuiOverlay = true;
    float motionAimSensitivity = 1.0f;
    bool useRecommendedResolution = true;
    bool srgbCorrection = true;
    bool depthSubmission = false;
    bool rawInputMode = true;
    bool autoInjectOnLaunch = false;
    float vrScaleFactor = 100.0f; // Game units per meter (e.g., 100 for UE cm scale, 1 for meters)
    int vrThreadPriority = THREAD_PRIORITY_HIGHEST; // VR render thread priority
    std::string shaderDir = ""; // Custom shader directory (empty = use moduleDir + "\\shaders")
    std::string modelDir = "";  // Custom model directory (empty = use moduleDir + "\\models")
    float depthBufferMaxSizeMultiplier = 16.0f; // Max depth buffer size as multiple of backbuffer (16.0 = 16x supersampling)
};

class ConfigManager {
public:
    static ConfigManager& GetInstance() {
        static ConfigManager instance;
        return instance;
    }

    // Loads configuration from a vrinject.json file located in the provided moduleDir.
    bool Load(const std::string& moduleDir);
    
    // Saves current configuration to vrinject.json.
    bool Save();

    const VRConfig& GetConfig() const { return m_config; }
    VRConfig& GetConfigMutable() { return m_config; }

private:
    ConfigManager() = default;
    ~ConfigManager() = default;

    VRConfig m_config;
    std::string m_configPath;
};

} // namespace vrinject
