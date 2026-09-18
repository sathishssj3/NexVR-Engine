#include "memory_scanner/page_scanner.h"
#include "memory_scanner/pointer_chain_resolver.h"
#include "core/logger.h"
#include "core/subsystem_context.h"
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
    SubsystemContext::Get().GetPointerChainResolver()->SetModuleBounds(m_mainModuleBase, m_mainModuleSize);
    LOG_INFO("PageScanner initialized. Base: %p, Size: %zx", m_mainModuleBase, m_mainModuleSize);

    // Fast-path: check if a valid pointer chain was cached from a previous session
    char exePath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) > 0) {
        std::string fullPath(exePath);
        size_t lastSlash = fullPath.find_last_of("\\/");
        std::string exeName = (lastSlash != std::string::npos) ? fullPath.substr(lastSlash + 1) : fullPath;
        
        PointerChain cachedChain;
        if (SubsystemContext::Get().GetPointerChainResolver()->LoadCache(exeName, cachedChain)) {
            uint8_t* ptr = SubsystemContext::Get().GetPointerChainResolver()->DereferenceChain(cachedChain);
            if (ptr) {
                float testMat[16];
                if (seh::SafeReadMemory(ptr, testMat, sizeof(testMat))) {
                    if (IsValidViewMatrixFloat(testMat)) {
                        LOG_INFO("PageScanner: Fast-path instant camera lock using cached pointer chain at %p!", ptr);
                        MemoryMatrixCandidate cand;
                        cand.address = ptr;
                        cand.isDoublePrecision = cachedChain.isDoublePrecision;
                        cand.isViewMatrix = true;
                        cand.isProjectionMatrix = false;
                        
                        std::lock_guard<std::mutex> lock(m_candidatesMutex);
                        m_dynamicCandidates.push_back(cand);
                        m_candidatePointers.push_back(ptr);
                    }
                }
            }
        }
    }

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

std::vector<MemoryMatrixCandidate> PageScanner::GetDynamicCandidates() {
    std::vector<MemoryMatrixCandidate> result;
    if (m_candidatesMutex.try_lock()) {
        result = m_dynamicCandidates;
        m_candidatesMutex.unlock();
    }
    return result;
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
    if (!mat) return false;
    bool rowMajor = (std::abs(std::abs(mat[11]) - 1.0f) <= 0.05f && std::abs(mat[15]) <= 0.01f);
    bool colMajor = (std::abs(std::abs(mat[14]) - 1.0f) <= 0.05f && std::abs(mat[15]) <= 0.01f);
    if (!rowMajor && !colMajor) return false;
    if (mat[0] <= 0.05f || mat[0] > 20.0f) return false;
    if (mat[5] <= 0.05f || mat[5] > 20.0f) return false;
    for (int i = 0; i < 16; ++i) {
        if (!std::isfinite(mat[i])) return false;
    }
    return true;
}

bool PageScanner::IsValidProjectionMatrixDouble(const double* mat, float targetFov) {
    if (!mat) return false;
    bool rowMajor = (std::abs(std::abs(mat[11]) - 1.0) <= 0.05 && std::abs(mat[15]) <= 0.01);
    bool colMajor = (std::abs(std::abs(mat[14]) - 1.0) <= 0.05 && std::abs(mat[15]) <= 0.01);
    if (!rowMajor && !colMajor) return false;
    if (mat[0] <= 0.05 || mat[0] > 20.0) return false;
    if (mat[5] <= 0.05 || mat[5] > 20.0) return false;
    for (int i = 0; i < 16; ++i) {
        if (!std::isfinite(mat[i])) return false;
    }
    return true;
}

bool PageScanner::IsValidViewMatrixFloat(const float* mat) {
    if (!mat) return false;

    // Check row-major or col-major orthonormal rotation block
    float l0 = mat[0]*mat[0] + mat[1]*mat[1] + mat[2]*mat[2];
    float l1 = mat[4]*mat[4] + mat[5]*mat[5] + mat[6]*mat[6];
    float l2 = mat[8]*mat[8] + mat[9]*mat[9] + mat[10]*mat[10];
    bool rowsNormalized = (std::abs(l0 - 1.0f) < 0.12f && std::abs(l1 - 1.0f) < 0.12f && std::abs(l2 - 1.0f) < 0.12f);

    float c0 = mat[0]*mat[0] + mat[4]*mat[4] + mat[8]*mat[8];
    float c1 = mat[1]*mat[1] + mat[5]*mat[5] + mat[9]*mat[9];
    float c2 = mat[2]*mat[2] + mat[6]*mat[6] + mat[10]*mat[10];
    bool colsNormalized = (std::abs(c0 - 1.0f) < 0.12f && std::abs(c1 - 1.0f) < 0.12f && std::abs(c2 - 1.0f) < 0.12f);

    if (!rowsNormalized && !colsNormalized) return false;

    if (rowsNormalized) {
        float dot01 = mat[0]*mat[4] + mat[1]*mat[5] + mat[2]*mat[6];
        float dot12 = mat[4]*mat[8] + mat[5]*mat[9] + mat[6]*mat[10];
        float dot02 = mat[0]*mat[8] + mat[1]*mat[9] + mat[2]*mat[10];
        if (std::abs(dot01) > 0.12f || std::abs(dot12) > 0.12f || std::abs(dot02) > 0.12f) return false;
    } else {
        float dot01 = mat[0]*mat[1] + mat[4]*mat[5] + mat[8]*mat[9];
        float dot12 = mat[1]*mat[2] + mat[5]*mat[6] + mat[9]*mat[10];
        float dot02 = mat[0]*mat[2] + mat[4]*mat[6] + mat[8]*mat[10];
        if (std::abs(dot01) > 0.12f || std::abs(dot12) > 0.12f || std::abs(dot02) > 0.12f) return false;
    }

    float det = mat[0]*(mat[5]*mat[10] - mat[6]*mat[9]) -
                mat[1]*(mat[4]*mat[10] - mat[6]*mat[8]) +
                mat[2]*(mat[4]*mat[9] - mat[5]*mat[8]);
    if (std::abs(std::abs(det) - 1.0f) > 0.20f) return false;

    if (std::abs(mat[15] - 1.0f) > 0.08f) return false;

    for (int i = 0; i < 16; ++i) {
        if (!std::isfinite(mat[i])) return false;
    }
    return true;
}

bool PageScanner::IsValidViewMatrixDouble(const double* mat) {
    if (!mat) return false;

    double l0 = mat[0]*mat[0] + mat[1]*mat[1] + mat[2]*mat[2];
    double l1 = mat[4]*mat[4] + mat[5]*mat[5] + mat[6]*mat[6];
    double l2 = mat[8]*mat[8] + mat[9]*mat[9] + mat[10]*mat[10];
    bool rowsNormalized = (std::abs(l0 - 1.0) < 0.12 && std::abs(l1 - 1.0) < 0.12 && std::abs(l2 - 1.0) < 0.12);

    double c0 = mat[0]*mat[0] + mat[4]*mat[4] + mat[8]*mat[8];
    double c1 = mat[1]*mat[1] + mat[5]*mat[5] + mat[9]*mat[9];
    double c2 = mat[2]*mat[2] + mat[6]*mat[6] + mat[10]*mat[10];
    bool colsNormalized = (std::abs(c0 - 1.0) < 0.12 && std::abs(c1 - 1.0) < 0.12 && std::abs(c2 - 1.0) < 0.12);

    if (!rowsNormalized && !colsNormalized) return false;

    if (rowsNormalized) {
        double dot01 = mat[0]*mat[4] + mat[1]*mat[5] + mat[2]*mat[6];
        double dot12 = mat[4]*mat[8] + mat[5]*mat[9] + mat[6]*mat[10];
        double dot02 = mat[0]*mat[8] + mat[1]*mat[9] + mat[2]*mat[10];
        if (std::abs(dot01) > 0.12 || std::abs(dot12) > 0.12 || std::abs(dot02) > 0.12) return false;
    } else {
        double dot01 = mat[0]*mat[1] + mat[4]*mat[5] + mat[8]*mat[9];
        double dot12 = mat[1]*mat[2] + mat[5]*mat[6] + mat[9]*mat[10];
        double dot02 = mat[0]*mat[2] + mat[4]*mat[6] + mat[8]*mat[10];
        if (std::abs(dot01) > 0.12 || std::abs(dot12) > 0.12 || std::abs(dot02) > 0.12) return false;
    }

    double det = mat[0]*(mat[5]*mat[10] - mat[6]*mat[9]) -
                 mat[1]*(mat[4]*mat[10] - mat[6]*mat[8]) +
                 mat[2]*(mat[4]*mat[9] - mat[5]*mat[8]);
    if (std::abs(std::abs(det) - 1.0) > 0.20) return false;

    if (std::abs(mat[15] - 1.0) > 0.08) return false;

    for (int i = 0; i < 16; ++i) {
        if (!std::isfinite(mat[i])) return false;
    }
    return true;
}

void PageScanner::ScanDynamicHeaps() {
    auto lastSweepTime = std::chrono::high_resolution_clock::now();

    while (m_scanRunning) {
        auto sweepStart = std::chrono::high_resolution_clock::now();
        size_t bytesScanned = 0;
        int viewCandidates = 0;
        int projCandidates = 0;

        MEMORY_BASIC_INFORMATION mbi;
        uint8_t* currentAddress = nullptr;

        std::vector<MemoryMatrixCandidate> newDynamicCandidates;
        std::vector<uint8_t*> newCandidatePointers;
        const size_t MAX_CANDIDATES_PER_SWEEP = 512;

        while (m_scanRunning && newDynamicCandidates.size() < MAX_CANDIDATES_PER_SWEEP) {
            if (VirtualQuery(currentAddress, &mbi, sizeof(mbi)) == 0) break;

            bool isGuarded = (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0;
            bool isReadable = (mbi.Protect & (PAGE_READWRITE | PAGE_READONLY | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) != 0;
            if (mbi.State == MEM_COMMIT && !isGuarded && isReadable && 
                (mbi.Type == MEM_PRIVATE || mbi.Type == MEM_MAPPED)) {
                uint8_t* scanStart = static_cast<uint8_t*>(mbi.BaseAddress);
                uint8_t* scanEnd = scanStart + mbi.RegionSize - sizeof(double) * 16;
                
                const size_t CHUNK_SIZE = 4096;
                uint8_t buffer[CHUNK_SIZE];
                
                for (uint8_t* p = scanStart; p < scanEnd && newDynamicCandidates.size() < MAX_CANDIDATES_PER_SWEEP; p += CHUNK_SIZE) {
                    size_t readSize = (std::min)(static_cast<size_t>(CHUNK_SIZE), static_cast<size_t>(scanEnd - p + sizeof(double)*16));
                    if (seh::SafeReadMemory(p, buffer, readSize)) {
                        bytesScanned += readSize;
                        
                        // 16-byte alignment step for matrix scans
                        for (size_t i = 0; i + sizeof(float)*16 <= readSize && newDynamicCandidates.size() < MAX_CANDIDATES_PER_SWEEP; i += 16) {
                            float* fmat = reinterpret_cast<float*>(buffer + i);
                            uint8_t* actualAddr = p + i;

                            bool isView = IsValidViewMatrixFloat(fmat);
                            bool isProj = !isView && IsValidProjectionMatrixFloat(fmat, m_targetFov);

                            if (isView || isProj) {
                                MemoryMatrixCandidate cand;
                                cand.address = actualAddr;
                                cand.isDoublePrecision = false;
                                cand.isViewMatrix = isView;
                                cand.isProjectionMatrix = isProj;

                                newDynamicCandidates.push_back(cand);
                                newCandidatePointers.push_back(actualAddr);
                                if (isView) viewCandidates++; else projCandidates++;
                            }
                            
                            // Check Double64 on 32-byte boundaries
                            if (i % 32 == 0 && i + sizeof(double)*16 <= readSize && newDynamicCandidates.size() < MAX_CANDIDATES_PER_SWEEP) {
                                double* dmat = reinterpret_cast<double*>(buffer + i);
                                bool isViewD = IsValidViewMatrixDouble(dmat);
                                bool isProjD = !isViewD && IsValidProjectionMatrixDouble(dmat, m_targetFov);

                                if (isViewD || isProjD) {
                                    MemoryMatrixCandidate cand;
                                    cand.address = actualAddr;
                                    cand.isDoublePrecision = true;
                                    cand.isViewMatrix = isViewD;
                                    cand.isProjectionMatrix = isProjD;

                                    newDynamicCandidates.push_back(cand);
                                    newCandidatePointers.push_back(actualAddr);
                                    if (isViewD) viewCandidates++; else projCandidates++;
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
            m_dynamicCandidates = newDynamicCandidates;
            m_candidatePointers = newCandidatePointers;
        }

        auto sweepEnd = std::chrono::high_resolution_clock::now();
        auto msTaken = std::chrono::duration_cast<std::chrono::milliseconds>(sweepEnd - sweepStart).count();
        
        LOG_INFO("PageScanner: Sweep complete. Scanned %zu MB in %lld ms. Found %d View, %d Projection candidates.", 
                 bytesScanned / (1024 * 1024), msTaken, viewCandidates, projCandidates);

        for (int i = 0; i < 40 && m_scanRunning; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

} // namespace vrinject
