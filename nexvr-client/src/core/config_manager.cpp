#include "core/config_manager.h"
#include "core/logger.h"
#include <fstream>
#include <vector>
#include <shlobj.h>
#include <nlohmann/json.hpp>
#include <algorithm>

using json = nlohmann::json;

namespace vrinject {

bool ConfigManager::Load(const std::string& moduleDir, const std::string& explicitHostExeDir) {
    std::vector<std::string> candidatePaths;
    
    // 1. Check exact moduleDir
    if (!moduleDir.empty()) {
        std::string base = moduleDir;
        if (base.back() == '\\' || base.back() == '/') base.pop_back();
        candidatePaths.push_back(base + "\\vrinject.json");
        
        // Hierarchical parent directory searches (up to 3 levels up, e.g. Phoenix\Binaries\Win64 -> install root)
        std::string cur = base;
        for (int i = 0; i < 3; ++i) {
            size_t slash = cur.find_last_of("\\/");
            if (slash == std::string::npos || slash == 0) break;
            cur = cur.substr(0, slash);
            candidatePaths.push_back(cur + "\\vrinject.json");
        }
    }

    // 2. Check host process executable directory (e.g. Phoenix/Binaries/Win64/HogwartsLegacy.exe)
    std::string hostExeDir = explicitHostExeDir;
    if (hostExeDir.empty()) {
        char hostExeBuf[MAX_PATH] = {0};
        if (::GetModuleFileNameA(NULL, hostExeBuf, MAX_PATH) > 0) {
            std::string hostExeStr = hostExeBuf;
            std::string hostExeLower = hostExeStr;
            std::transform(hostExeLower.begin(), hostExeLower.end(), hostExeLower.begin(), ::tolower);
            // Skip automated discovery from test runner binary directory in unit tests
            if (hostExeLower.find("test_") == std::string::npos) {
                size_t lastSlash = hostExeStr.find_last_of("\\/");
                if (lastSlash != std::string::npos) {
                    hostExeDir = hostExeStr.substr(0, lastSlash);
                }
            }
        }
    }

    if (!hostExeDir.empty()) {
        if (hostExeDir.back() == '\\' || hostExeDir.back() == '/') hostExeDir.pop_back();
        candidatePaths.push_back(hostExeDir + "\\vrinject.json");

        // Hierarchical parent directory searches for host exe (up to 3 levels up)
        std::string cur = hostExeDir;
        for (int i = 0; i < 3; ++i) {
            size_t slash = cur.find_last_of("\\/");
            if (slash == std::string::npos || slash == 0) break;
            cur = cur.substr(0, slash);
            candidatePaths.push_back(cur + "\\vrinject.json");
        }
    }
    
    // 3. User local appdata fallback (e.g. %LOCALAPPDATA%\VRInject\vrinject.json)
    char appData[MAX_PATH]{};
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appData))) {
        candidatePaths.push_back(std::string(appData) + "\\VRInject\\vrinject.json");
    }

    std::string foundPath = "";
    for (const auto& path : candidatePaths) {
        int wLen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
        std::wstring wPath(wLen > 0 ? wLen : 1, L'\0');
        if (wLen > 0) MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wPath[0], wLen);

        std::ifstream testFile(wPath);
        if (testFile.is_open()) {
            foundPath = path;
            break;
        }
    }

    if (foundPath.empty()) {
        m_configPath = moduleDir + "\\vrinject.json";
        LOG_INFO("Config file not found in module directory or parent trees. Creating default at %s.", m_configPath.c_str());
        return Save();
    }

    m_configPath = foundPath;
    int wLen = MultiByteToWideChar(CP_UTF8, 0, m_configPath.c_str(), -1, nullptr, 0);
    std::wstring wConfigPath(wLen > 0 ? wLen : 1, L'\0');
    if (wLen > 0) MultiByteToWideChar(CP_UTF8, 0, m_configPath.c_str(), -1, &wConfigPath[0], wLen);

    std::ifstream file(wConfigPath);
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
        m_config.vrScaleFactor = j.value("vrScaleFactor", 100.0f);
        m_config.vrThreadPriority = j.value("vrThreadPriority", THREAD_PRIORITY_HIGHEST);
        m_config.shaderDir = j.value("shaderDir", "");
        m_config.modelDir = j.value("modelDir", "");
        m_config.depthBufferMaxSizeMultiplier = j.value("depthBufferMaxSizeMultiplier", 16.0f);

        // Per-game engine profile overrides
        m_config.engineType = j.value("engine", "");
        m_config.api = j.value("api", "");
        m_config.apiType = m_config.api;
        m_config.matrixPrecision = j.value("matrixPrecision", "Float32");
        if (j.contains("reverseZ")) {
            m_config.hasReverseZOverride = true;
            m_config.reverseZ = j.value("reverseZ", true);
        }
        if (j.contains("rowMajorMatrices")) {
            m_config.hasRowMajorOverride = true;
            m_config.rowMajorMatrices = j.value("rowMajorMatrices", true);
        }
        
        LOG_INFO("Configuration loaded successfully from %s", m_configPath.c_str());
        return true;
    } catch (const nlohmann::json::exception& e) {
        LOG_WARN("Config parse error — using defaults: %s", e.what());
        m_config = VRConfig{}; // reset to safe defaults
        return false;
    } catch (const std::exception& e) {
        // Catch std::out_of_range, std::invalid_argument, etc. from json parsing
        LOG_WARN("Config parse error (std::exception) — using defaults: %s", e.what());
        m_config = VRConfig{};
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
        
        j["useRecommendedResolution"] = m_config.useRecommendedResolution;
        j["srgbCorrection"] = m_config.srgbCorrection;
        j["depthSubmission"] = m_config.depthSubmission;
        j["rawInputMode"] = m_config.rawInputMode;
        j["autoInjectOnLaunch"] = m_config.autoInjectOnLaunch;
        j["vrScaleFactor"] = m_config.vrScaleFactor;
        j["vrThreadPriority"] = m_config.vrThreadPriority;
        j["shaderDir"] = m_config.shaderDir;
        j["modelDir"] = m_config.modelDir;
        j["depthBufferMaxSizeMultiplier"] = m_config.depthBufferMaxSizeMultiplier;

        if (!m_config.engineType.empty()) j["engine"] = m_config.engineType;
        if (!m_config.api.empty()) j["api"] = m_config.api;
        if (!m_config.matrixPrecision.empty()) j["matrixPrecision"] = m_config.matrixPrecision;
        if (m_config.hasReverseZOverride) j["reverseZ"] = m_config.reverseZ;
        if (m_config.hasRowMajorOverride) j["rowMajorMatrices"] = m_config.rowMajorMatrices;
        
        // FIX (portable): Use wide path for saving as well.
        int wLen = MultiByteToWideChar(CP_UTF8, 0, m_configPath.c_str(), -1, nullptr, 0);
        std::wstring wConfigPath(wLen > 0 ? wLen : 1, L'\0');
        if (wLen > 0) MultiByteToWideChar(CP_UTF8, 0, m_configPath.c_str(), -1, &wConfigPath[0], wLen);

        std::ofstream file(wConfigPath);
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
