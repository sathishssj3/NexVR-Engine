#include "memory_scanner.h"
#include "logger.h"
#include <Psapi.h>
#include <sstream>

namespace vrinject {

bool MemoryScanner::Initialize() {
    auto info = GetModuleInfo("");
    if (!info.baseAddress) {
        LOG_ERROR("MemoryScanner failed to get main module info.");
        return false;
    }
    m_mainModuleBase = info.baseAddress;
    m_mainModuleSize = info.size;
    LOG_INFO("MemoryScanner initialized. Base: %p, Size: %zx", m_mainModuleBase, m_mainModuleSize);
    return true;
}

MemoryScanner::ModuleInfo MemoryScanner::GetModuleInfo(const std::string& moduleName) {
    ModuleInfo info;
    HMODULE hModule = nullptr;
    if (moduleName.empty()) {
        hModule = GetModuleHandle(nullptr);
    } else {
        hModule = GetModuleHandleA(moduleName.c_str());
    }

    if (hModule) {
        MODULEINFO moduleInfo;
        if (GetModuleInformation(GetCurrentProcess(), hModule, &moduleInfo, sizeof(moduleInfo))) {
            info.baseAddress = static_cast<uint8_t*>(moduleInfo.lpBaseOfDll);
            info.size = moduleInfo.SizeOfImage;
        }
    }
    return info;
}

uint8_t* MemoryScanner::ScanSignature(const std::string& pattern, const std::string& moduleName) {
    ModuleInfo info = moduleName.empty() ? ModuleInfo{m_mainModuleBase, m_mainModuleSize} : GetModuleInfo(moduleName);
    
    if (!info.baseAddress) {
        return nullptr;
    }

    // Convert pattern string into byte arrays and masks
    std::vector<int> patternBytes;
    std::istringstream iss(pattern);
    std::string byteStr;
    while (iss >> byteStr) {
        if (byteStr == "?" || byteStr == "??") {
            patternBytes.push_back(-1);
        } else {
            patternBytes.push_back(std::stoi(byteStr, nullptr, 16));
        }
    }

    const size_t patternSize = patternBytes.size();
    if (info.size < patternSize || patternSize == 0) {
        return nullptr;
    }

    const int* patternData = patternBytes.data();
    uint8_t* searchBase = info.baseAddress;
    uint8_t* searchEnd = info.baseAddress + info.size - patternSize;

    MEMORY_BASIC_INFORMATION mbi;
    uint8_t* currentAddress = searchBase;

    while (currentAddress < searchEnd) {
        if (VirtualQuery(currentAddress, &mbi, sizeof(mbi)) == 0) {
            break; // Query failed, stop scanning
        }

        // Check if page is committed and readable
        if (mbi.State == MEM_COMMIT && 
            (mbi.Protect == PAGE_EXECUTE_READ || 
             mbi.Protect == PAGE_EXECUTE_READWRITE || 
             mbi.Protect == PAGE_READONLY || 
             mbi.Protect == PAGE_READWRITE)) {

            uint8_t* pageStart = static_cast<uint8_t*>(mbi.BaseAddress);
            // Ensure we start searching from at least currentAddress and don't exceed searchEnd
            uint8_t* scanStart = (pageStart > currentAddress) ? pageStart : currentAddress;
            uint8_t* scanEnd = pageStart + mbi.RegionSize - patternSize;
            if (scanEnd > searchEnd) scanEnd = searchEnd;

            for (uint8_t* p = scanStart; p <= scanEnd; ++p) {
                bool found = true;
                for (size_t j = 0; j < patternSize; ++j) {
                    if (patternData[j] != -1 && p[j] != static_cast<uint8_t>(patternData[j])) {
                        found = false;
                        break;
                    }
                }
                if (found) {
                    return p;
                }
            }
        }

        currentAddress = static_cast<uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
    }

    return nullptr;
}

uint8_t* MemoryScanner::ResolveRIP(uint8_t* instructionAddress, uint32_t instructionSize, uint32_t offsetPosition) {
    if (!instructionAddress) return nullptr;

    // FIX #20: Validate that the 4-byte RIP offset field is fully within a
    // committed, readable page before dereferencing. A false-positive signature
    // match at the end of a page would otherwise cause an access violation.
    uint8_t* readPtr = instructionAddress + offsetPosition;
    MEMORY_BASIC_INFORMATION mbi = {};
    if (VirtualQuery(readPtr, &mbi, sizeof(mbi)) == 0 ||
        mbi.State != MEM_COMMIT ||
        (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ |
                        PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) == 0 ||
        (mbi.Protect & PAGE_GUARD) != 0) {
        LOG_WARN("MemoryScanner::ResolveRIP: address %p is not in a readable page, skipping.", readPtr);
        return nullptr;
    }
    // Ensure all 4 bytes of the int32 are within the same committed region.
    uintptr_t regionEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    if (reinterpret_cast<uintptr_t>(readPtr) + sizeof(int32_t) > regionEnd) {
        LOG_WARN("MemoryScanner::ResolveRIP: int32 at %p spans a page boundary, skipping.", readPtr);
        return nullptr;
    }

    // In x64, a RIP-relative address is an int32 added to the address of the NEXT instruction.
    int32_t offset = *reinterpret_cast<int32_t*>(readPtr);
    return instructionAddress + instructionSize + offset;
}

} // namespace vrinject
#include <chrono>
#include <cmath>
#include "seh_shield.h"

namespace vrinject {

void MemoryScanner::StartDynamicScan(float targetFov) {
    if (m_scanRunning) return;
    m_targetFov = targetFov;
    m_scanRunning = true;
    m_scanThread = std::thread(&MemoryScanner::ScanDynamicHeaps, this);
    LOG_INFO("MemoryScanner: Async dynamic scan thread started (Target FOV: %.1f).", targetFov);
}

void MemoryScanner::StopDynamicScan() {
    if (m_scanRunning) {
        m_scanRunning = false;
        if (m_scanThread.joinable()) {
            m_scanThread.join();
        }
        LOG_INFO("MemoryScanner: Async dynamic scan thread stopped.");
    }
}

std::vector<uint8_t*> MemoryScanner::GetCandidateStaticPointers() {
    std::vector<uint8_t*> result;
    if (m_candidatesMutex.try_lock()) {
        result = m_candidatePointers;
        m_candidatesMutex.unlock();
    }
    return result;
}

bool MemoryScanner::IsValidProjectionMatrixFloat(const float* mat, float targetFov) {
    // Quick heuristic: mat[11] is typically -1.0 or 1.0 (or very close) for perspective projections
    if (std::abs(std::abs(mat[11]) - 1.0f) > 0.01f) return false;
    // mat[15] is typically 0.0 for perspective
    if (std::abs(mat[15]) > 0.001f) return false;
    
    // Check horizontal FOV heuristic based on mat[0] (or 1/tan(fov/2))
    // We expect the value to be somewhat sane (e.g. between 0.5 and 3.0)
    if (mat[0] <= 0.1f || mat[0] > 10.0f) return false;
    if (mat[5] <= 0.1f || mat[5] > 10.0f) return false;

    return true;
}

bool MemoryScanner::IsValidProjectionMatrixDouble(const double* mat, float targetFov) {
    if (std::abs(std::abs(mat[11]) - 1.0) > 0.01) return false;
    if (std::abs(mat[15]) > 0.001) return false;
    if (mat[0] <= 0.1 || mat[0] > 10.0) return false;
    if (mat[5] <= 0.1 || mat[5] > 10.0) return false;
    return true;
}

uint8_t* MemoryScanner::ResolvePointerChain(uint8_t* targetAddress, int maxDepth) {
    // Simple 1-level static pointer backtrace for now.
    // In a full implementation, this searches for pointers TO targetAddress,
    // then checks if that pointer is inside m_mainModuleBase.
    
    // For the sake of this milestone, we'll scan the .data / .bss sections of main module.
    // If we find an 8-byte pointer matching targetAddress, we return its static address.
    if (!m_mainModuleBase) return nullptr;

    MEMORY_BASIC_INFORMATION mbi;
    uint8_t* currentAddress = m_mainModuleBase;
    uint8_t* searchEnd = m_mainModuleBase + m_mainModuleSize;

    while (currentAddress < searchEnd) {
        if (VirtualQuery(currentAddress, &mbi, sizeof(mbi)) == 0) break;

        if (mbi.State == MEM_COMMIT && (mbi.Protect == PAGE_READWRITE)) {
            uint8_t* scanStart = static_cast<uint8_t*>(mbi.BaseAddress);
            uint8_t* scanEnd = scanStart + mbi.RegionSize - sizeof(uintptr_t);
            
            for (uint8_t* p = scanStart; p <= scanEnd; p += sizeof(uintptr_t)) {
                uintptr_t candidatePtr = 0;
                if (seh::SafeReadMemory(p, &candidatePtr, sizeof(candidatePtr))) {
                    if (candidatePtr == reinterpret_cast<uintptr_t>(targetAddress)) {
                        LOG_INFO("MemoryScanner: Resolved 1-level static pointer at %p pointing to %p", p, targetAddress);
                        return p;
                    }
                }
            }
        }
        currentAddress = static_cast<uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
    }
    return nullptr;
}

void MemoryScanner::ScanDynamicHeaps() {
    auto lastSweepTime = std::chrono::high_resolution_clock::now();

    while (m_scanRunning) {
        auto sweepStart = std::chrono::high_resolution_clock::now();
        size_t bytesScanned = 0;
        int floatCandidates = 0;
        int doubleCandidates = 0;

        MEMORY_BASIC_INFORMATION mbi;
        uint8_t* currentAddress = nullptr; // Start from 0x0

        std::vector<uint8_t*> newCandidates;

        while (m_scanRunning) {
            if (VirtualQuery(currentAddress, &mbi, sizeof(mbi)) == 0) break;

            if (mbi.State == MEM_COMMIT && mbi.Protect == PAGE_READWRITE && (mbi.Type == MEM_PRIVATE || mbi.Type == MEM_MAPPED)) {
                uint8_t* scanStart = static_cast<uint8_t*>(mbi.BaseAddress);
                uint8_t* scanEnd = scanStart + mbi.RegionSize - sizeof(double) * 16;
                
                // Read the whole page safely if possible, or chunk it.
                // For performance, we assume standard scanning, but wrap in SEH per Gap 1.
                // To avoid calling SafeReadMemory per float, we can read chunks.
                const size_t CHUNK_SIZE = 4096;
                uint8_t buffer[CHUNK_SIZE];
                
                for (uint8_t* p = scanStart; p < scanEnd; p += CHUNK_SIZE) {
                    size_t readSize = (std::min)(static_cast<size_t>(CHUNK_SIZE), static_cast<size_t>(scanEnd - p + sizeof(double)*16));
                    if (seh::SafeReadMemory(p, buffer, readSize)) {
                        bytesScanned += readSize;
                        
                        // Scan buffer for float[16] and double[16]
                        for (size_t i = 0; i < readSize - sizeof(double)*16; i += sizeof(float)) {
                            // Check float
                            float* fmat = reinterpret_cast<float*>(buffer + i);
                            if (IsValidProjectionMatrixFloat(fmat, m_targetFov)) {
                                floatCandidates++;
                                uint8_t* actualAddr = p + i;
                                uint8_t* staticPtr = ResolvePointerChain(actualAddr);
                                if (staticPtr) newCandidates.push_back(staticPtr);
                            }
                            
                            // Check double (aligned to 8 bytes)
                            if (i % sizeof(double) == 0) {
                                double* dmat = reinterpret_cast<double*>(buffer + i);
                                if (IsValidProjectionMatrixDouble(dmat, m_targetFov)) {
                                    doubleCandidates++;
                                    uint8_t* actualAddr = p + i;
                                    uint8_t* staticPtr = ResolvePointerChain(actualAddr);
                                    if (staticPtr) newCandidates.push_back(staticPtr);
                                }
                            }
                        }
                    }
                }
            }
            
            currentAddress = static_cast<uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
            
            // Yield to avoid freezing the system
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        if (m_scanRunning) {
            std::lock_guard<std::mutex> lock(m_candidatesMutex);
            m_candidatePointers = newCandidates;
        }

        auto sweepEnd = std::chrono::high_resolution_clock::now();
        auto msTaken = std::chrono::duration_cast<std::chrono::milliseconds>(sweepEnd - sweepStart).count();
        
        LOG_INFO("MemoryScanner: Sweep complete. Scanned %zu MB in %lld ms. Candidates: %d float, %d double.", 
                 bytesScanned / (1024 * 1024), msTaken, floatCandidates, doubleCandidates);

        // Sleep before next full sweep
        for (int i = 0; i < 50 && m_scanRunning; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

} // namespace vrinject
