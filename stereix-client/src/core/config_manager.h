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
    bool srgbCorrection = false;
    float contrast = 1.0f;
    float saturation = 1.0f;
    float brightness = 1.0f;
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

    // P0.2: Floating Curved HUD in World Space
    bool curvedHud = true;               // Enable curved HUD reprojection for corner elements
    float hudDistance = 1.8f;             // Virtual HUD distance in meters
    float hudCurvature = 0.35f;           // Cylinder curvature (0 = flat, 1 = half cylinder)

    // P1.1: Cutscene Auto-Theater Mode
    bool cutsceneTheater = true;          // Detect camera cuts and switch to cinema screen
    float theaterDistance = 5.0f;          // Virtual cinema screen distance in meters
    float theaterCutThreshold = 120.0f;   // Degrees/sec rotational jerk threshold for cut detection

    // P1.2: Dynamic Comfort Vignette
    bool comfortVignette = true;          // Enable dynamic peripheral FOV restriction
    float vignetteStrength = 0.6f;        // Max vignette intensity [0, 1]
    float vignetteOnset = 45.0f;          // Angular velocity (deg/s) where vignette starts

    // P2: 6DOF Camera Decoupling & Horizon Lock
    bool horizonLock = false;             // Lock camera roll to gravity (stabilize horizon)
    float horizonLockStrength = 0.85f;    // Blend factor for roll dampening [0, 1]
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
