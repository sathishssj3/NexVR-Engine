#include "memory_scanner/signature_scanner.h"

namespace vrinject {

uint8_t* SignatureScanner::ScanSignature(const std::string& pattern, const std::string& moduleName) {
    ModuleInfo info = GetModuleInfo(moduleName);
    
    if (!info.baseAddress) {
        return nullptr;
    }

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
            break;
        }

        if (mbi.State == MEM_COMMIT && 
            (mbi.Protect == PAGE_EXECUTE_READ || 
             mbi.Protect == PAGE_EXECUTE_READWRITE || 
             mbi.Protect == PAGE_READONLY || 
             mbi.Protect == PAGE_READWRITE)) {

            uint8_t* pageStart = static_cast<uint8_t*>(mbi.BaseAddress);
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

} // namespace vrinject
