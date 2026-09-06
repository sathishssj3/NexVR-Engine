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
    
    // Per-game engine profile overrides (isolates games from global heuristic drift)
    std::string engineType = ""; // Optional override: "UnrealEngine4", "UnrealEngine5", "Unity", "Generic"
    std::string api = "";        // Optional API override: "DX11", "DX12", "Vulkan"
    std::string apiType = "";    // Canonical alias matching engineType naming
    std::string matrixPrecision = "Float32"; // "Float32", "Double64", "Auto"
    bool hasReverseZOverride = false;
    bool reverseZ = true;
    bool hasRowMajorOverride = false;
    bool rowMajorMatrices = true;
};

class ConfigManager {
public:

    // Loads configuration from a vrinject.json file located in the provided moduleDir,
    // with hierarchical fallback to hostExeDir or the host process executable directory.
    bool Load(const std::string& moduleDir, const std::string& hostExeDir = "");
    
    // Saves current configuration to vrinject.json.
    bool Save();

    const VRConfig& GetConfig() const { return m_config; }
    VRConfig& GetConfigMutable() { return m_config; }

public:
    ConfigManager() = default;
    ~ConfigManager() = default;

    VRConfig m_config;
    std::string m_configPath;
};

} // namespace vrinject
