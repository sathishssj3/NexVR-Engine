#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <string>
#include <vector>
#include <cstdint>
#include <sstream>
#include <Psapi.h>

namespace vrinject {

class SignatureScanner {
public:

    uint8_t* ScanSignature(const std::string& pattern, const std::string& moduleName = "");

    SignatureScanner() = default;

private:

    struct ModuleInfo {
        uint8_t* baseAddress = nullptr;
        size_t size = 0;
    };
    
    ModuleInfo GetModuleInfo(const std::string& moduleName) {
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
};

} // namespace vrinject
