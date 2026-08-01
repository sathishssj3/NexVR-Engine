#include "core/runtime_state.h"
#include "core/hook_manager.h"
#include "core/logger.h"
#include "core/seh_shield.h"
#include "core/drm_manager.h"
#include <windows.h>
#include <shlobj.h>

namespace vrinject {

void RuntimeState::TransitionTo(RuntimePhase newPhase) {
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_phase.store(newPhase);
    }
    m_stateCv.notify_all();
}

void RuntimeState::WaitForPhase(RuntimePhase phase) {
    std::unique_lock<std::mutex> lock(m_stateMutex);
    m_stateCv.wait(lock, [this, phase]() {
        return m_phase.load() == phase;
    });
}

void RuntimeState::OnDllProcessAttach(void* hModule) {
    m_hModule = hModule;
    TransitionTo(RuntimePhase::Attached);

    m_workerThread = std::thread(&RuntimeState::BackgroundInitialize, this);
    m_workerThread.detach();
}

void RuntimeState::OnDllProcessDetach() {
    TransitionTo(RuntimePhase::Stopping);

    std::thread teardownThread(&RuntimeState::BackgroundTeardown, this);
    teardownThread.detach();
}

void RuntimeState::BackgroundInitialize() {
    TransitionTo(RuntimePhase::Initializing);
    
    // Pin module
    HMODULE pinnedModule = nullptr;
    ::GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCSTR>(&RuntimeState::Get),
        &pinnedModule);

    // Initialize logger
    char dllDir[MAX_PATH]{};
    if (::GetModuleFileNameA(static_cast<HMODULE>(m_hModule), dllDir, MAX_PATH)) {
        char* lastSlash = std::strrchr(dllDir, '\\');
        if (lastSlash) *(lastSlash + 1) = '\0';
    }

    char logPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, logPath))) {
        strcat_s(logPath, "\\VRInject");
        ::CreateDirectoryA(logPath, nullptr);
        strcat_s(logPath, "\\vrinject.log");
    } else {
        strcpy_s(logPath, dllDir);
        strcat_s(logPath, "vrinject.log");
    }
    Logger::Init(logPath);
    seh::RegisterVehShield(static_cast<HMODULE>(m_hModule));

    LOG_INFO("========================================");
    LOG_INFO("VRInject Framework v0.1 - Initializing");
    LOG_INFO("========================================");
    
    DiagnosticContext::Get().PostEvent(DiagnosticLevel::Info, "Runtime", "Starting background initialization");
    
    if (!TrialManager::GetInstance().InitializeTrial()) {
        LOG_ERROR("Trial validation failed - shutting down");
        TransitionTo(RuntimePhase::Error);
        return;
    }
    
    bool success = HookManager::Get().InitializeHooks();
    
    if (success) {
        DiagnosticContext::Get().PostEvent(DiagnosticLevel::Info, "Runtime", "Initialization complete");
        TransitionTo(RuntimePhase::Running);
    } else {
        DiagnosticContext::Get().PostEvent(DiagnosticLevel::Error, "Runtime", "Initialization failed");
        TransitionTo(RuntimePhase::Error);
    }
}

void RuntimeState::BackgroundTeardown() {
    DiagnosticContext::Get().PostEvent(DiagnosticLevel::Info, "Runtime", "Starting background teardown");
    
    HookManager::Get().ShutdownHooks();
    seh::UnregisterVehShield();
    
    DiagnosticContext::Get().PostEvent(DiagnosticLevel::Info, "Runtime", "Teardown complete");
    TransitionTo(RuntimePhase::Stopped);
}

} // namespace vrinject
