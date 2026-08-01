#pragma once
#include <atomic>
#include <thread>
#include <mutex>
#include <functional>
#include "core/diagnostic_context.h"

namespace vrinject {

enum class RuntimePhase {
    Uninitialized,
    Attached,
    Initializing,
    Running,
    Stopping,
    Stopped,
    Error
};

class RuntimeState {
public:
    static RuntimeState& Get() {
        static RuntimeState instance;
        return instance;
    }

    void OnDllProcessAttach(void* hModule);
    void OnDllProcessDetach();
    
    RuntimePhase GetPhase() const { return m_phase.load(); }
    void* GetModuleHandle() const { return m_hModule; }

    // Used for tests to wait for a phase
    void WaitForPhase(RuntimePhase phase);

private:
    RuntimeState() : m_phase(RuntimePhase::Uninitialized), m_hModule(nullptr) {}

    void BackgroundInitialize();
    void BackgroundTeardown();

    std::atomic<RuntimePhase> m_phase;
    void* m_hModule;
    std::thread m_workerThread;
    
    std::mutex m_stateMutex;
    std::condition_variable m_stateCv;

    void TransitionTo(RuntimePhase newPhase);
};

} // namespace vrinject
