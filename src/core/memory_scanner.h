#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <vector>
#include <string>
#include <cstdint>
#include <thread>
#include <atomic>
#include <mutex>
#include <DirectXMath.h>

namespace vrinject {

class MemoryScanner {
public:
    static MemoryScanner& Get() {
        static MemoryScanner instance;
        return instance;
    }

    // Initialize with the base address and size of the main executable module
    bool Initialize();

    // Scan for an Array of Bytes (AoB) signature.
    // Example pattern: "48 8B 05 ? ? ? ? 48 8B 88 ? ? ? ?" (where ? is a wildcard)
    uint8_t* ScanSignature(const std::string& pattern, const std::string& moduleName = "");

    // Resolves a relative instruction pointer (RIP) to an absolute address
    // Common in x64 for resolving globals like GEngine: lea rcx, [rip+offset]
    uint8_t* ResolveRIP(uint8_t* instructionAddress, uint32_t instructionSize, uint32_t offsetFromInstructionEnd);



    // Starts the background thread to scan for dynamic projection matrices
    void StartDynamicScan(float targetFov);

    // Stops the background scanning thread
    void StopDynamicScan();

    // Retrieves the latest resolved static base pointers that point (via chains) to candidates
    // CameraDeltaTracker will read from these.
    std::vector<uint8_t*> GetCandidateStaticPointers();

private:
    MemoryScanner() = default;
    ~MemoryScanner() { StopDynamicScan(); }

    struct ModuleInfo {
        uint8_t* baseAddress = nullptr;
        size_t size = 0;
    };
    
    ModuleInfo GetModuleInfo(const std::string& moduleName);
    
    void ScanDynamicHeaps();
    uint8_t* ResolvePointerChain(uint8_t* targetAddress, int maxDepth = 3);
    bool IsValidProjectionMatrixFloat(const float* mat, float targetFov);
    bool IsValidProjectionMatrixDouble(const double* mat, float targetFov);

    uint8_t* m_mainModuleBase = nullptr;
    size_t m_mainModuleSize = 0;
    float m_targetFov = 90.0f;

    std::thread m_scanThread;
    std::atomic<bool> m_scanRunning{false};

    // Lockless (or try_lock) communication to the render thread
    std::mutex m_candidatesMutex;
    std::vector<uint8_t*> m_candidatePointers;
};

} // namespace vrinject
