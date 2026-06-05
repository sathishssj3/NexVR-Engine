#include "config_manager.h"
#include "logger.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <algorithm>

using json = nlohmann::json;

namespace vrinject {

bool ConfigManager::Load(const std::string& moduleDir) {
    m_configPath = moduleDir + "\\vrinject.json";
    
    std::ifstream file(m_configPath);
    if (!file.is_open()) {
        LOG_INFO("Config file not found at %s. Creating default config.", m_configPath.c_str());
        return Save();
    }

    try {
        json j;
        file >> j;
        
        m_config.ipd = j.value("ipd", 0.064f);
        m_config.convergence = j.value("convergence", 10.0f);
        m_config.resolutionScale = j.value("resolutionScale", 1.0f);
        m_config.enableNeuralInpainter = j.value("enableNeuralInpainter", true);
        m_config.enableImGuiOverlay = j.value("enableImGuiOverlay", true);
        
        m_config.motionAimSensitivity = std::clamp(
            j.value("motionAimSensitivity", 1.0f), 0.1f, 10.0f);
            
        m_config.useRecommendedResolution = j.value("useRecommendedResolution", true);
        m_config.srgbCorrection = j.value("srgbCorrection", true);
        m_config.depthSubmission = j.value("depthSubmission", false);
        m_config.rawInputMode = j.value("rawInputMode", true);
        m_config.autoInjectOnLaunch = j.value("autoInjectOnLaunch", false);
        
        LOG_INFO("Configuration loaded successfully from %s", m_configPath.c_str());
        return true;
    } catch (const nlohmann::json::exception& e) {
        LOG_WARN("Config parse error — using defaults: %s", e.what());
        m_config = VRConfig{}; // reset to safe defaults
        return false;
    }
}

bool ConfigManager::Save() {
    if (m_configPath.empty()) return false;
    
    try {
        json j;
        j["ipd"] = m_config.ipd;
        j["convergence"] = m_config.convergence;
        j["resolutionScale"] = m_config.resolutionScale;
        j["enableNeuralInpainter"] = m_config.enableNeuralInpainter;
        j["enableImGuiOverlay"] = m_config.enableImGuiOverlay;
        j["motionAimSensitivity"] = m_config.motionAimSensitivity;
        
        std::ofstream file(m_configPath);
        if (file.is_open()) {
            file << j.dump(4);
            return true;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to save config file: %s", e.what());
    }
    return false;
}

} // namespace vrinject
