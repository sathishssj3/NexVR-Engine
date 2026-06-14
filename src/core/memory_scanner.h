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

    // Sweeps PAGE_READWRITE memory in the background to discover dynamically allocated camera matrices
    void StartBackgroundMatrixScan();
    void StopBackgroundMatrixScan();

private:
    MemoryScanner() = default;

    struct ModuleInfo {
        uint8_t* baseAddress = nullptr;
        size_t size = 0;
    };

    ModuleInfo GetModuleInfo(const std::string& moduleName);
    
    uint8_t* m_mainModuleBase = nullptr;
    size_t m_mainModuleSize = 0;

    std::thread m_scanThread;
    std::atomic<bool> m_isScanning{false};
    void ScanLoop();
};

} // namespace vrinject
