#pragma once
#include <windows.h>
#include <string>
#include <atomic>
#include <thread>

namespace vrinject {

class TrialManager {
public:
    static TrialManager& GetInstance() {
        static TrialManager instance;
        return instance;
    }

    bool CheckAndEnforceTrial();
    void Shutdown();

private:
    TrialManager() = default;
    ~TrialManager();

    std::string GetExecutableName();
    bool ReadRegistryData();
    void WriteRegistryData();
    void TrialEnforcementThread();
    void TriggerExpiration(const char* reason);

    DWORD m_playtimeMinutes = 0;
    DWORD m_gameCount = 0;
    std::string m_games[3];
    std::string m_currentExe;

    std::atomic<bool> m_running{false};
    std::thread m_enforcementThread;
};

} // namespace vrinject
