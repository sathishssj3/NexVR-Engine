#pragma once
#include "engine_detector.h"
#include <vector>
#include <functional>
#include <mutex>

namespace vrinject {

enum class HookPhase {
    Uninitialized,
    Prepared,
    Validated,
    Installed,
    Verified,
    Committed,
    Failed,
    RolledBack
};

class HookManager {
public:
    static HookManager& Get() {
        static HookManager instance;
        return instance;
    }

    // Existing methods that map to the new transaction flow
    bool InitializeHooks();
    void ShutdownHooks();

    // Transactional Lifecycle
    bool Prepare();
    bool Validate();
    bool Install();
    bool Verify();
    bool Commit();
    void Rollback();
    
    HookPhase GetPhase() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_phase;
    }

private:
    HookManager() : m_phase(HookPhase::Uninitialized) {}
    
    HookPhase m_phase;
    std::vector<std::function<void()>> m_rollbackStack;
    mutable std::recursive_mutex m_mutex;
};

} // namespace vrinject
