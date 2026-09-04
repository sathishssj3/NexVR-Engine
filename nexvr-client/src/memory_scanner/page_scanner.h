#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <vector>
#include <cstdint>
#include <thread>
#include <atomic>
#include <mutex>
#include <string>

namespace vrinject {

class PageScanner {
public:

    bool Initialize();

    // Starts the background thread to scan for dynamic projection matrices
    void StartDynamicScan(float targetFov);

    // Stops the background scanning thread
    void StopDynamicScan();

    // Retrieves the latest resolved static base pointers that point (via chains) to candidates
    std::vector<uint8_t*> GetCandidateStaticPointers();

    PageScanner() = default;
    ~PageScanner() { StopDynamicScan(); }

private:

    struct ModuleInfo {
        uint8_t* baseAddress = nullptr;
        size_t size = 0;
    };
    
    ModuleInfo GetModuleInfo(const std::string& moduleName);
    
    void ScanDynamicHeaps();
    bool IsValidProjectionMatrixFloat(const float* mat, float targetFov);
    bool IsValidProjectionMatrixDouble(const double* mat, float targetFov);

    uint8_t* m_mainModuleBase = nullptr;
    size_t m_mainModuleSize = 0;
    float m_targetFov = 90.0f;

    std::thread m_scanThread;
    std::atomic<bool> m_scanRunning{false};

    std::mutex m_candidatesMutex;
    std::vector<uint8_t*> m_candidatePointers;
};

} // namespace vrinject
