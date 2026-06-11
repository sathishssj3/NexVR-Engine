#pragma once

namespace vrinject {
namespace ue {

class UnrealHook {
public:
    static UnrealHook& Get() {
        static UnrealHook instance;
        return instance;
    }

    bool Initialize();
    void Shutdown();

private:
    UnrealHook() = default;
    
    // Type definitions for the target functions
    // Note: Signatures change based on engine version. These are generic placeholders.
    typedef bool (*GetProjectionData_t)(void* pLocalPlayer, void* pViewport, int stereoPass, void* pProjectionData);

    // Original function pointers (trampolines)
    GetProjectionData_t m_originalGetProjectionData = nullptr;

    // Static detour function
    static bool DetourGetProjectionData(void* pLocalPlayer, void* pViewport, int stereoPass, void* pProjectionData);
};

} // namespace ue
} // namespace vrinject
