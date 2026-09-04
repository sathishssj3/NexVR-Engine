#include "core/iat_hook.h"
#include "core/logger.h"
#include <DbgHelp.h>

#pragma comment(lib, "Dbghelp.lib")

namespace vrinject {
namespace core {

bool IATHook::InstallHook(HMODULE targetModule, const char* importModule, const char* importName, void* detourFunc, void** originalFuncOut) {
    if (!targetModule || !importModule || !importName || !detourFunc) return false;

    ULONG size = 0;
    PIMAGE_IMPORT_DESCRIPTOR importDesc = (PIMAGE_IMPORT_DESCRIPTOR)ImageDirectoryEntryToData(
        targetModule, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &size);

    if (!importDesc) {
        LOG_WARN("IATHook: No import directory found in target module.");
        return false;
    }

    while (importDesc->Name) {
        LPCSTR moduleName = (LPCSTR)((PBYTE)targetModule + importDesc->Name);
        if (_stricmp(moduleName, importModule) == 0) {
            PIMAGE_THUNK_DATA origFirstThunk = (PIMAGE_THUNK_DATA)((PBYTE)targetModule + importDesc->OriginalFirstThunk);
            PIMAGE_THUNK_DATA firstThunk = (PIMAGE_THUNK_DATA)((PBYTE)targetModule + importDesc->FirstThunk);

            while (origFirstThunk->u1.AddressOfData) {
                if (!(origFirstThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG)) {
                    PIMAGE_IMPORT_BY_NAME importByName = (PIMAGE_IMPORT_BY_NAME)((PBYTE)targetModule + origFirstThunk->u1.AddressOfData);
                    if (strcmp((const char*)importByName->Name, importName) == 0) {
                        
                        if (originalFuncOut) {
                            *originalFuncOut = (void*)firstThunk->u1.Function;
                        }

                        // Change memory protection to allow writing
                        DWORD oldProtect;
                        if (!VirtualProtect(&firstThunk->u1.Function, sizeof(PVOID), PAGE_READWRITE, &oldProtect)) {
                            LOG_ERROR("IATHook: Failed to change memory protection.");
                            return false;
                        }

                        // Overwrite the pointer
                        firstThunk->u1.Function = (ULONGLONG)detourFunc;

                        // Restore original protection
                        VirtualProtect(&firstThunk->u1.Function, sizeof(PVOID), oldProtect, &oldProtect);

                        LOG_INFO("IATHook: Successfully hooked %s!%s", importModule, importName);
                        return true;
                    }
                }
                origFirstThunk++;
                firstThunk++;
            }
        }
        importDesc++;
    }

    LOG_WARN("IATHook: Function %s!%s not found in IAT.", importModule, importName);
    return false;
}

bool IATHook::RemoveHook(HMODULE targetModule, const char* importModule, const char* importName, void* originalFunc) {
    if (!originalFunc) return false;
    // Removing the hook is essentially just installing the original function pointer back
    return InstallHook(targetModule, importModule, importName, originalFunc, nullptr);
}

} // namespace core
} // namespace vrinject
