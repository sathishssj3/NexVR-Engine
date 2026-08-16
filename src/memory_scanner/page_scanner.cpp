#include "memory_scanner/page_scanner.h"
#include "memory_scanner/pointer_chain_resolver.h"
#include "core/logger.h"
#include "core/seh_shield.h"
#include <Psapi.h>
#include <chrono>
#include <cmath>
#include <algorithm>

namespace vrinject {

bool PageScanner::Initialize() {
    auto info = GetModuleInfo("");
    if (!info.baseAddress) {
        LOG_ERROR("PageScanner failed to get main module info.");
        return false;
    }
    m_mainModuleBase = info.baseAddress;
    m_mainModuleSize = info.size;
    PointerChainResolver::Get().SetModuleBounds(m_mainModuleBase, m_mainModuleSize);
    LOG_INFO("PageScanner initialized. Base: %p, Size: %zx", m_mainModuleBase, m_mainModuleSize);
    return true;
}

PageScanner::ModuleInfo PageScanner::GetModuleInfo(const std::string& moduleName) {
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

void PageScanner::StartDynamicScan(float targetFov) {
    if (m_scanRunning) return;
    m_targetFov = targetFov;
    m_scanRunning = true;
    m_scanThread = std::thread(&PageScanner::ScanDynamicHeaps, this);
    LOG_INFO("PageScanner: Async dynamic scan thread started (Target FOV: %.1f).", targetFov);
}

void PageScanner::StopDynamicScan() {
    if (m_scanRunning) {
        m_scanRunning = false;
        if (m_scanThread.joinable()) {
            m_scanThread.join();
        }
        LOG_INFO("PageScanner: Async dynamic scan thread stopped.");
    }
}

std::vector<uint8_t*> PageScanner::GetCandidateStaticPointers() {
    std::vector<uint8_t*> result;
    if (m_candidatesMutex.try_lock()) {
        result = m_candidatePointers;
        m_candidatesMutex.unlock();
    }
    return result;
}

bool PageScanner::IsValidProjectionMatrixFloat(const float* mat, float targetFov) {
    if (std::abs(std::abs(mat[11]) - 1.0f) > 0.01f) return false;
    if (std::abs(mat[15]) > 0.001f) return false;
    if (mat[0] <= 0.1f || mat[0] > 10.0f) return false;
    if (mat[5] <= 0.1f || mat[5] > 10.0f) return false;
    return true;
}

bool PageScanner::IsValidProjectionMatrixDouble(const double* mat, float targetFov) {
    if (std::abs(std::abs(mat[11]) - 1.0) > 0.01) return false;
    if (std::abs(mat[15]) > 0.001) return false;
    if (mat[0] <= 0.1 || mat[0] > 10.0) return false;
    if (mat[5] <= 0.1 || mat[5] > 10.0) return false;
    return true;
}

void PageScanner::ScanDynamicHeaps() {
    auto lastSweepTime = std::chrono::high_resolution_clock::now();

    while (m_scanRunning) {
        auto sweepStart = std::chrono::high_resolution_clock::now();
        size_t bytesScanned = 0;
        int floatCandidates = 0;
        int doubleCandidates = 0;

        MEMORY_BASIC_INFORMATION mbi;
        uint8_t* currentAddress = nullptr;

        std::vector<uint8_t*> newCandidates;

        while (m_scanRunning) {
            if (VirtualQuery(currentAddress, &mbi, sizeof(mbi)) == 0) break;

            if (mbi.State == MEM_COMMIT && mbi.Protect == PAGE_READWRITE && (mbi.Type == MEM_PRIVATE || mbi.Type == MEM_MAPPED)) {
                uint8_t* scanStart = static_cast<uint8_t*>(mbi.BaseAddress);
                uint8_t* scanEnd = scanStart + mbi.RegionSize - sizeof(double) * 16;
                
                const size_t CHUNK_SIZE = 4096;
                uint8_t buffer[CHUNK_SIZE];
                
                for (uint8_t* p = scanStart; p < scanEnd; p += CHUNK_SIZE) {
                    size_t readSize = (std::min)(static_cast<size_t>(CHUNK_SIZE), static_cast<size_t>(scanEnd - p + sizeof(double)*16));
                    if (seh::SafeReadMemory(p, buffer, readSize)) {
                        bytesScanned += readSize;
                        
                        for (size_t i = 0; i < readSize - sizeof(double)*16; i += sizeof(float)) {
                            float* fmat = reinterpret_cast<float*>(buffer + i);
                            if (IsValidProjectionMatrixFloat(fmat, m_targetFov)) {
                                floatCandidates++;
                                uint8_t* actualAddr = p + i;
                                uint8_t* staticPtr = PointerChainResolver::Get().ResolvePointerChain(actualAddr);
                                if (staticPtr) newCandidates.push_back(staticPtr);
                            }
                            
                            if (i % sizeof(double) == 0) {
                                double* dmat = reinterpret_cast<double*>(buffer + i);
                                if (IsValidProjectionMatrixDouble(dmat, m_targetFov)) {
                                    doubleCandidates++;
                                    uint8_t* actualAddr = p + i;
                                    uint8_t* staticPtr = PointerChainResolver::Get().ResolvePointerChain(actualAddr);
                                    if (staticPtr) newCandidates.push_back(staticPtr);
                                }
                            }
                        }
                    }
                }
            }
            
            currentAddress = static_cast<uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        if (m_scanRunning) {
            std::lock_guard<std::mutex> lock(m_candidatesMutex);
            m_candidatePointers = newCandidates;
        }

        auto sweepEnd = std::chrono::high_resolution_clock::now();
        auto msTaken = std::chrono::duration_cast<std::chrono::milliseconds>(sweepEnd - sweepStart).count();
        
        LOG_INFO("PageScanner: Sweep complete. Scanned %zu MB in %lld ms. Candidates: %d float, %d double.", 
                 bytesScanned / (1024 * 1024), msTaken, floatCandidates, doubleCandidates);

        for (int i = 0; i < 50 && m_scanRunning; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

} // namespace vrinject
