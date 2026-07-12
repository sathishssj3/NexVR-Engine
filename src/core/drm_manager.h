#pragma once
#include <windows.h>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace vrinject {

class TrialManager {
public:
    static TrialManager& GetInstance() {
        static TrialManager instance;
        return instance;
    }

    // Call from worker thread AFTER DllMain returns
    bool InitializeTrial();
    void Shutdown();

private:
    TrialManager() = default;
    ~TrialManager();

    std::string GetExecutableName();
    bool ReadRegistryData();
    void WriteRegistryData();
    void TriggerExpiration(const char* reason);

    DWORD m_playtimeMinutes = 0;
    DWORD m_gameCount = 0;
    std::string m_games[10];
    std::string m_currentExe;

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_expiryPending{false}; // FIX #12: set instead of calling MessageBoxA under lock
    std::recursive_mutex m_mutex;
    std::condition_variable_any m_stopCondition;
    bool m_initialized = false;
};

} // namespace vrinject
