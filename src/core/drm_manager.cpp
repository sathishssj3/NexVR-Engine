#define _CRT_SECURE_NO_WARNINGS
#include "drm_manager.h"
#include "../core/logger.h"
#include <vector>
#include <stdio.h>

namespace vrinject {

#define REG_PATH "Software\\Microsoft\\Windows\\CurrentVersion\\VRInjectData"
#define MAX_PLAYTIME_MINUTES 180
#define MAX_GAMES 10

TrialManager::~TrialManager() {
    Shutdown();
}

std::string TrialManager::GetExecutableName() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string fullPath(path);
    size_t lastSlash = fullPath.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        return fullPath.substr(lastSlash + 1);
    }
    return fullPath;
}

bool TrialManager::ReadRegistryData() {
    HKEY hKey;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, REG_PATH, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS) {
        return false;
    }

    DWORD type = REG_DWORD;
    DWORD size = sizeof(DWORD);
    if (RegQueryValueExA(hKey, "T_Val", NULL, &type, (LPBYTE)&m_playtimeMinutes, &size) != ERROR_SUCCESS) {
        m_playtimeMinutes = 0;
    }

    type = REG_DWORD;
    size = sizeof(DWORD);
    if (RegQueryValueExA(hKey, "G_Count", NULL, &type, (LPBYTE)&m_gameCount, &size) != ERROR_SUCCESS) {
        m_gameCount = 0;
    }

    for (DWORD i = 0; i < m_gameCount && i < MAX_GAMES; ++i) {
        char keyName[16];
        snprintf(keyName, sizeof(keyName), "G_Name%d", i);
        char buffer[MAX_PATH];
        DWORD bufSize = sizeof(buffer);
        if (RegQueryValueExA(hKey, keyName, NULL, NULL, (LPBYTE)buffer, &bufSize) == ERROR_SUCCESS) {
            m_games[i] = buffer;
        }
    }

    RegCloseKey(hKey);
    return true;
}

void TrialManager::WriteRegistryData() {
    HKEY hKey;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, REG_PATH, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExA(hKey, "T_Val", 0, REG_DWORD, (const BYTE*)&m_playtimeMinutes, sizeof(DWORD));
        RegSetValueExA(hKey, "G_Count", 0, REG_DWORD, (const BYTE*)&m_gameCount, sizeof(DWORD));
        
        for (DWORD i = 0; i < m_gameCount && i < MAX_GAMES; ++i) {
            char keyName[16];
            snprintf(keyName, sizeof(keyName), "G_Name%d", i);
            RegSetValueExA(hKey, keyName, 0, REG_SZ, (const BYTE*)m_games[i].c_str(), m_games[i].length() + 1);
        }
        RegCloseKey(hKey);
    }
}

void TrialManager::TriggerExpiration(const char* reason) {
    LOG_INFO("TrialManager: Expired! Reason: %s", reason);
    MessageBoxA(NULL, "NexVR Beta Trial Expired.\nYou have reached the 3-hour or 10-game limit.\nPlease purchase the full version to continue.", "NexVR Beta", MB_OK | MB_ICONWARNING);
    ExitProcess(0);
}

bool TrialManager::CheckAndEnforceTrial() {
    m_currentExe = GetExecutableName();
    
    // Ignore injector and cli
    if (m_currentExe == "injector.exe" || m_currentExe == "vr-inject-cli.exe") {
        return true;
    }

    if (!ReadRegistryData()) {
        WriteRegistryData(); // Init
    }

    if (m_playtimeMinutes >= MAX_PLAYTIME_MINUTES) {
        TriggerExpiration("Time limit reached (180 mins)");
        return false;
    }

    bool isKnownGame = false;
    for (DWORD i = 0; i < m_gameCount; ++i) {
        if (m_games[i] == m_currentExe) {
            isKnownGame = true;
            break;
        }
    }

    if (!isKnownGame) {
        if (m_gameCount >= MAX_GAMES) {
            TriggerExpiration("Game limit reached (10 unique games)");
            return false;
        } else {
            // Register new game
            m_games[m_gameCount] = m_currentExe;
            m_gameCount++;
            WriteRegistryData();
            LOG_INFO("TrialManager: Registered new game %s (%d/%d)", m_currentExe.c_str(), m_gameCount, MAX_GAMES);
        }
    }

    // Start background enforcement thread
    if (!m_running) {
        m_running = true;
        m_enforcementThread = std::thread(&TrialManager::TrialEnforcementThread, this);
        m_enforcementThread.detach(); // Let it run indefinitely
    }

    return true;
}

void TrialManager::TrialEnforcementThread() {
    while (m_running) {
        Sleep(60000); // Wait 1 minute
        
        m_playtimeMinutes++;
        WriteRegistryData();
        
        LOG_INFO("TrialManager: Playtime updated to %d/%d minutes", m_playtimeMinutes, MAX_PLAYTIME_MINUTES);
        
        if (m_playtimeMinutes >= MAX_PLAYTIME_MINUTES) {
            TriggerExpiration("Time limit reached while playing");
        }
    }
}

void TrialManager::Shutdown() {
    m_running = false;
}

} // namespace vrinject
