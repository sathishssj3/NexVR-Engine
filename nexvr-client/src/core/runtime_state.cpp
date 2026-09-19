#include "core/runtime_state.h"
#include "core/hook_manager.h"
#include "core/logger.h"
#include "core/subsystem_context.h"
#include "core/config_manager.h"
#include "core/seh_shield.h"
#include "core/drm_manager.h"
#include "core/version.h"
#include <windows.h>
#include <shlobj.h>
#include <chrono>

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

    // R5: Bounded wait to prevent DLL unloading while teardown is running
    // The existing m_workerThread will wake up, run teardown, and transition to Stopped.
    std::unique_lock<std::mutex> lock(m_stateMutex);
    m_stateCv.wait_for(lock, std::chrono::milliseconds(100), [this]() {
        return m_phase.load() == RuntimePhase::Stopped;
    });
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

    char exePath[MAX_PATH]{};
    ::GetModuleFileNameA(nullptr, exePath, MAX_PATH);

    char logPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, logPath))) {
        strcat_s(logPath, "\\VRInject");
        ::CreateDirectoryA(logPath, nullptr);
        strcat_s(logPath, "\\vrinject.log");
    } else {
        strcpy_s(logPath, dllDir);
        strcat_s(logPath, "vrinject.log");
    }
    
    auto logger = std::make_shared<FileLogger>();
    logger->Init(logPath);

    // Also attach secondary log in dll directory
    char dllLogPath[MAX_PATH];
    strcpy_s(dllLogPath, dllDir);
    strcat_s(dllLogPath, "vrinject.log");
    logger->AddSecondaryPath(dllLogPath);

    // Also attach secondary log in game executable directory if different
    char exeDirLogPath[MAX_PATH];
    strcpy_s(exeDirLogPath, exePath);
    char* exeSlash = std::strrchr(exeDirLogPath, '\\');
    if (exeSlash) {
        *(exeSlash + 1) = '\0';
        strcat_s(exeDirLogPath, "vrinject.log");
        logger->AddSecondaryPath(exeDirLogPath);
    }

    auto config = std::make_shared<ConfigManager>();
    config->Load(dllDir);
    SubsystemContext::Get().Initialize(logger, config);

    seh::RegisterVehShield(static_cast<HMODULE>(m_hModule));

    LOG_INFO("========================================");
    LOG_INFO("NexVR Engine v" NEXVR_ENGINE_VERSION " - Initializing");
    LOG_INFO("========================================");
    LOG_INFO("[DIAGNOSTICS] Host Process: %s", exePath);
    LOG_INFO("[DIAGNOSTICS] Module DLL Dir: %s", dllDir);
    LOG_INFO("[DIAGNOSTICS] Primary Log Path: %s", logPath);

    const auto& cfg = config->GetConfig();
    LOG_INFO("[DIAGNOSTICS] Config: IPD=%.3fm, Convergence=%.1fm, ResScale=%.2f", cfg.ipd, cfg.convergence, cfg.resolutionScale);
    LOG_INFO("[DIAGNOSTICS] Color Calibration: Brightness=%.2f, Contrast=%.2f, Saturation=%.2f, sRGB=%s",
             cfg.brightness, cfg.contrast, cfg.saturation, cfg.srgbCorrection ? "On" : "Off");
    LOG_INFO("[DIAGNOSTICS] Engine Profile: %s | API Override: %s",
             cfg.engineType.empty() ? "Auto" : cfg.engineType.c_str(),
             cfg.api.empty() ? "Auto" : cfg.api.c_str());
    
    SubsystemContext::Get().GetDiagnosticContext()->PostEvent(DiagnosticLevel::Info, "Runtime", "Starting background initialization");
    
    if (!TrialManager::GetInstance().InitializeTrial()) {
        LOG_ERROR("Trial validation failed - shutting down");
        TransitionTo(RuntimePhase::Error);
        return;
    }
    
    LOG_DEBUG("Waiting for remote injection thread to exit...");
    Sleep(1000); // Prevent MH_EnableHook from deadlocking against the exiting injection thread

    LOG_DEBUG("About to call HookManager::InitializeHooks()...");
    bool success = HookManager::Get().InitializeHooks();
    LOG_DEBUG("HookManager::InitializeHooks() returned: %s", success ? "true" : "false");
    
    if (success) {
        LOG_INFO("[OK] HookManager: Detours & Memory Interceptors Active");
        SubsystemContext::Get().GetDiagnosticContext()->PostEvent(DiagnosticLevel::Info, "Runtime", "Initialization complete");
        TransitionTo(RuntimePhase::Running);
    } else {
        SubsystemContext::Get().GetDiagnosticContext()->PostEvent(DiagnosticLevel::Error, "Runtime", "Initialization failed");
        TransitionTo(RuntimePhase::Error);
    }

    // R5: Park the existing worker thread here, waiting for OnDllProcessDetach to set Stopping
    {
        std::unique_lock<std::mutex> lock(m_stateMutex);
        m_stateCv.wait(lock, [this]() {
            return m_phase.load() == RuntimePhase::Stopping || m_phase.load() == RuntimePhase::Stopped;
        });
    }

    // Perform teardown on the original worker thread
    BackgroundTeardown();
}

void RuntimeState::BackgroundTeardown() {
    SubsystemContext::Get().GetDiagnosticContext()->PostEvent(DiagnosticLevel::Info, "Runtime", "Starting background teardown");
    
    HookManager::Get().ShutdownHooks();
    seh::UnregisterVehShield();
    
    SubsystemContext::Get().Shutdown();

    SubsystemContext::Get().GetDiagnosticContext()->PostEvent(DiagnosticLevel::Info, "Runtime", "Teardown complete");
    TransitionTo(RuntimePhase::Stopped);
}

} // namespace vrinject
