#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <string>

namespace vrinject {
namespace core {

class IATHook {
public:
    // Installs an IAT hook on the specified target module, intercepting calls to importName in importModule
    // Example: InstallHook(GetModuleHandle(NULL), "d3d11.dll", "D3D11CreateDevice", myDetour, &originalFunc)
    static bool InstallHook(HMODULE targetModule, const char* importModule, const char* importName, void* detourFunc, void** originalFuncOut);
    
    // Restores a previously installed IAT hook
    static bool RemoveHook(HMODULE targetModule, const char* importModule, const char* importName, void* originalFunc);
};

} // namespace core
} // namespace vrinject
