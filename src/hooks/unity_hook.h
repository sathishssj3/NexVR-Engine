#pragma once

namespace vrinject {
namespace unity {

class UnityHook {
public:
    static UnityHook& Get() {
        static UnityHook instance;
        return instance;
    }

    bool Initialize();
    void Shutdown();

private:
    UnityHook() = default;

    // Type definitions for IL2CPP camera functions
    // Unity IL2CPP functions typically take (this, args..., MethodInfo* method)
    typedef void (*set_worldToCameraMatrix_t)(void* camera_this, void* matrix, void* method_info);
    typedef void (*set_projectionMatrix_t)(void* camera_this, void* matrix, void* method_info);

    // Original function pointers
    set_worldToCameraMatrix_t m_originalSetWorldToCameraMatrix = nullptr;
    set_projectionMatrix_t m_originalSetProjectionMatrix = nullptr;

    // Static detour functions
    static void DetourSetWorldToCameraMatrix(void* camera_this, void* matrix, void* method_info);
    static void DetourSetProjectionMatrix(void* camera_this, void* matrix, void* method_info);
};

} // namespace unity
} // namespace vrinject
