#define _CRT_SECURE_NO_WARNINGS
#include "drm_manager.h"
#include "../core/logger.h"
#include <vector>
#include <stdio.h>
#include <chrono>
#include <atomic>

namespace vrinject {

#define REG_PATH "Software\\VRInject\\DRM"
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
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
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
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
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
    // FIX #12: Do NOT call MessageBoxA while holding m_mutex.
    // Set the flag here; the actual dialog is shown after releasing the lock
    // in the enforcement thread or in the caller that checked InitializeTrial.
    m_expiryPending = true;
}

bool TrialManager::InitializeTrial() {
    m_initialized = true;
    return true;
}



void TrialManager::Shutdown() {
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_running = false;
    }
    m_stopCondition.notify_all();
}

} // namespace vrinject
