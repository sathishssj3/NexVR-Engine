#pragma once

#include <cstdint>

namespace vrinject {
namespace unity {

// Unity's Matrix4x4 layout (column-major, matches DirectX convention)
struct Matrix4x4 {
    float m[4][4];
};

class UnityHook {
public:
    static UnityHook& Get() {
        static UnityHook instance;
        return instance;
    }

    bool Initialize();
    void Shutdown();

    bool IsHooked() const { return m_isHooked; }

private:
    UnityHook() = default;

    // ========================================================================
    // IL2CPP calling convention:
    //   returnType Method(void* this, args..., void* methodInfo)
    // For static methods:
    //   returnType Method(args..., void* methodInfo)
    // ========================================================================

    // Camera.get_main() -> Camera*  (static, 0 real args)
    typedef void* (*Camera_get_main_t)(void* methodInfo);
    Camera_get_main_t m_originalGetMain = nullptr;
    static void* DetourGetMain(void* methodInfo);

    // Camera.set_worldToCameraMatrix(Matrix4x4 value)
    // Instance method: (this, Matrix4x4*, methodInfo)
    typedef void (*Camera_set_worldToCameraMatrix_t)(void* camera_this, Matrix4x4* matrix, void* methodInfo);
    Camera_set_worldToCameraMatrix_t m_originalSetWorldToCameraMatrix = nullptr;
    static void DetourSetWorldToCameraMatrix(void* camera_this, Matrix4x4* matrix, void* methodInfo);

    // Camera.set_projectionMatrix(Matrix4x4 value)
    typedef void (*Camera_set_projectionMatrix_t)(void* camera_this, Matrix4x4* matrix, void* methodInfo);
    Camera_set_projectionMatrix_t m_originalSetProjectionMatrix = nullptr;
    static void DetourSetProjectionMatrix(void* camera_this, Matrix4x4* matrix, void* methodInfo);

    bool m_isHooked = false;

    // The main camera instance we track
    void* m_mainCamera = nullptr;
};

} // namespace unity
} // namespace vrinject
