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
    // In x64, a RIP-relative address is usually an int32 added to the address of the NEXT instruction.
    int32_t offset = *reinterpret_cast<int32_t*>(instructionAddress + offsetPosition);
    return instructionAddress + instructionSize + offset;
}

void MemoryScanner::StartBackgroundMatrixScan() {
    if (!m_isScanning) {
        m_isScanning = true;
        m_scanThread = std::thread(&MemoryScanner::ScanLoop, this);
        LOG_INFO("MemoryScanner: Background heuristic scan started.");
    }
}

void MemoryScanner::StopBackgroundMatrixScan() {
    if (m_isScanning) {
        m_isScanning = false;
        if (m_scanThread.joinable()) {
            m_scanThread.join();
        }
        LOG_INFO("MemoryScanner: Background heuristic scan stopped.");
    }
}

void MemoryScanner::ScanLoop() {
    // Basic implementation of PAGE_READWRITE scanning for matrices
    // In a real scenario, this would incrementally scan heaps.
    // For now, we will wait, as true deep memory scanning is highly engine-dependent and computationally expensive.
    // The core heuristic logic relies on the OnConstantBufferUpdate hook to feed candidates.
    while (m_isScanning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

} // namespace vrinject
