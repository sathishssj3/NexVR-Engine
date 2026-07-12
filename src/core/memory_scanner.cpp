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
