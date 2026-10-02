#include "memory_scanner/pointer_chain_resolver.h"
#include "core/logger.h"
#include "core/seh_shield.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <shlobj.h>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace vrinject {

void PointerChainResolver::SetModuleBounds(uint8_t* base, size_t size) {
    m_mainModuleBase = base;
    m_mainModuleSize = size;
}

uint8_t* PointerChainResolver::ResolveRIP(uint8_t* instructionAddress, uint32_t instructionSize, uint32_t offsetPosition) {
    if (!instructionAddress) return nullptr;

    uint8_t* readPtr = instructionAddress + offsetPosition;
    MEMORY_BASIC_INFORMATION mbi = {};
    if (VirtualQuery(readPtr, &mbi, sizeof(mbi)) == 0 ||
        mbi.State != MEM_COMMIT ||
        (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ |
                        PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) == 0 ||
        (mbi.Protect & PAGE_GUARD) != 0) {
        LOG_DEBUG("PointerChainResolver: address %p is not in a readable page, skipping.", readPtr);
        return nullptr;
    }

    uintptr_t regionEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    if (reinterpret_cast<uintptr_t>(readPtr) + sizeof(int32_t) > regionEnd) {
        LOG_DEBUG("PointerChainResolver: int32 at %p spans a page boundary, skipping.", readPtr);
        return nullptr;
    }

    int32_t offset = *reinterpret_cast<int32_t*>(readPtr);
    return instructionAddress + instructionSize + offset;
}

uint8_t* PointerChainResolver::ResolvePointerChain(uint8_t* targetAddress, int maxDepth) {
    PointerChain chain;
    if (ResolveChain(targetAddress, chain, 0)) {
        return m_mainModuleBase + chain.baseOffset;
    }
    return nullptr;
}

bool PointerChainResolver::ResolveChain(uint8_t* targetAddress, PointerChain& outChain, int maxOffset) {
    if (!m_mainModuleBase || !targetAddress) return false;

    MEMORY_BASIC_INFORMATION mbi;
    uint8_t* currentAddress = m_mainModuleBase;
    uint8_t* searchEnd = m_mainModuleBase + m_mainModuleSize;

    uintptr_t targetVal = reinterpret_cast<uintptr_t>(targetAddress);

    while (currentAddress < searchEnd) {
        if (VirtualQuery(currentAddress, &mbi, sizeof(mbi)) == 0) break;

        if (mbi.State == MEM_COMMIT && (mbi.Protect & (PAGE_READWRITE | PAGE_READONLY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE))) {
            uint8_t* regionStart = static_cast<uint8_t*>(mbi.BaseAddress);
            uint8_t* regionEnd = regionStart + mbi.RegionSize;

            uint8_t* scanStart = (std::max)(currentAddress, regionStart);
            uint8_t* scanEnd = (std::min)(searchEnd, regionEnd);

            if (scanEnd >= scanStart + sizeof(uintptr_t)) {
                for (uint8_t* p = scanStart; p <= scanEnd - sizeof(uintptr_t); p += sizeof(uintptr_t)) {
                    uintptr_t candidatePtr = 0;
                    if (seh::SafeReadMemory(p, &candidatePtr, sizeof(candidatePtr))) {
                        if (candidatePtr != 0 && candidatePtr <= targetVal && (targetVal - candidatePtr) <= static_cast<size_t>(maxOffset)) {
                            outChain.baseOffset = p - m_mainModuleBase;
                            outChain.offsets = { static_cast<int32_t>(targetVal - candidatePtr) };
                            outChain.valid = true;
                            LOG_INFO("PointerChainResolver: Resolved static base Game+%zx -> offset +0x%x pointing to %p", 
                                     outChain.baseOffset, outChain.offsets[0], targetAddress);
                            return true;
                        }
                    }
                }
            }
        }
        currentAddress = static_cast<uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
    }
    return false;
}

uint8_t* PointerChainResolver::DereferenceChain(const PointerChain& chain) {
    if (!chain.valid || !m_mainModuleBase) return nullptr;

    uint8_t* current = m_mainModuleBase + chain.baseOffset;
    uintptr_t targetPtr = 0;
    if (!seh::SafeReadMemory(current, &targetPtr, sizeof(targetPtr)) || targetPtr == 0) {
        return nullptr;
    }

    current = reinterpret_cast<uint8_t*>(targetPtr);

    for (size_t i = 0; i < chain.offsets.size(); ++i) {
        if (i + 1 < chain.offsets.size()) {
            current += chain.offsets[i];
            uintptr_t nextPtr = 0;
            if (!seh::SafeReadMemory(current, &nextPtr, sizeof(nextPtr)) || nextPtr == 0) {
                return nullptr;
            }
            current = reinterpret_cast<uint8_t*>(nextPtr);
        } else {
            current += chain.offsets[i];
        }
    }

    return current;
}

std::string PointerChainResolver::GetCachePath() const {
    char appData[MAX_PATH]{};
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appData))) {
        std::string dir = std::string(appData) + "\\NexVR Engine";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        return dir + "\\camera_cache.json";
    }
    return "camera_cache.json";
}

bool PointerChainResolver::SaveCache(const std::string& exeName, const PointerChain& chain) {
    if (exeName.empty() || !chain.valid) return false;

    std::string path = GetCachePath();
    json root = json::object();

    std::ifstream inFile(path);
    if (inFile.is_open()) {
        try { inFile >> root; } catch (...) { root = json::object(); }
        inFile.close();
    }

    json entry = json::object();
    entry["baseOffset"] = chain.baseOffset;
    entry["offsets"] = chain.offsets;
    entry["isDoublePrecision"] = chain.isDoublePrecision;
    root[exeName] = entry;

    std::ofstream outFile(path);
    if (outFile.is_open()) {
        outFile << root.dump(2);
        LOG_INFO("PointerChainResolver: Saved camera pointer chain cache for %s to %s", exeName.c_str(), path.c_str());
        return true;
    }
    return false;
}

bool PointerChainResolver::LoadCache(const std::string& exeName, PointerChain& outChain) {
    if (exeName.empty()) return false;

    std::string path = GetCachePath();
    std::ifstream inFile(path);
    if (!inFile.is_open()) return false;

    try {
        json root;
        inFile >> root;
        if (root.contains(exeName)) {
            const auto& entry = root[exeName];
            outChain.baseOffset = entry.value("baseOffset", uintptr_t(0));
            outChain.offsets = entry.value("offsets", std::vector<int32_t>{});
            outChain.isDoublePrecision = entry.value("isDoublePrecision", false);
            outChain.valid = (outChain.baseOffset != 0);
            if (outChain.valid) {
                LOG_INFO("PointerChainResolver: Loaded cached camera pointer chain for %s (Base+%zx)", 
                         exeName.c_str(), outChain.baseOffset);
                return true;
            }
        }
    } catch (...) {}
    return false;
}

} // namespace vrinject
