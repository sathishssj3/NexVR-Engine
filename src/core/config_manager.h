#pragma once

#include <string>

namespace vrinject {

struct VRConfig {
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
