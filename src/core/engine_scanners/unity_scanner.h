#pragma once

#include <windows.h>
#include <string>
#include <cstdint>

namespace vrinject {
namespace engine_scanners {

// Which Unity scripting backend is in use
enum class UnityBackend {
    Unknown,
    Mono,       // mono-2.0-bdwgc.dll (older Unity or debug builds)
    IL2CPP      // GameAssembly.dll (production builds)
};

class UnityScanner {
public:
    static UnityScanner& Get() {
        static UnityScanner instance;
        return instance;
    }

    bool Initialize();
    void Shutdown();

    bool IsUnityEngine() const { return m_isUnity; }
    UnityBackend GetBackend() const { return m_backend; }

    // Returns true if successfully hooked UnityEngine.Camera
    bool HookCamera();

    // ========================================================================
    // IL2CPP API function pointers (resolved at runtime)
    // ========================================================================
    
    // Domain / Thread management
    typedef void* (*il2cpp_domain_get_t)();
    typedef void* (*il2cpp_thread_attach_t)(void* domain);
    typedef void  (*il2cpp_thread_detach_t)(void* thread);

    // Type system
    typedef void* (*il2cpp_domain_get_assemblies_t)(void* domain, size_t* count);
    typedef void* (*il2cpp_assembly_get_image_t)(void* assembly);
    typedef void* (*il2cpp_class_from_name_t)(void* image, const char* namespaze, const char* name);
    typedef void* (*il2cpp_class_get_methods_t)(void* klass, void** iter);
    typedef void* (*il2cpp_class_get_method_from_name_t)(void* klass, const char* name, int argsCount);
    typedef const char* (*il2cpp_method_get_name_t)(void* method);
    typedef void* (*il2cpp_method_get_param_t)(void* method, int index);
    
    // Image/assembly info
    typedef const char* (*il2cpp_image_get_name_t)(void* image);
    typedef size_t (*il2cpp_image_get_class_count_t)(void* image);
    typedef void* (*il2cpp_image_get_class_t)(void* image, size_t index);
    typedef const char* (*il2cpp_class_get_name_t)(void* klass);
    typedef const char* (*il2cpp_class_get_namespace_t)(void* klass);

    // ========================================================================
    // Mono API function pointers (resolved at runtime)
    // ========================================================================
    typedef void* (*mono_domain_get_t)();
    typedef void* (*mono_thread_attach_t)(void* domain);
    typedef void* (*mono_domain_assembly_open_t)(void* domain, const char* name);
    typedef void* (*mono_assembly_get_image_t)(void* assembly);
    typedef void* (*mono_class_from_name_t)(void* image, const char* namespaze, const char* name);
    typedef void* (*mono_class_get_method_from_name_t)(void* klass, const char* name, int paramCount);
    typedef void* (*mono_compile_method_t)(void* method);

    // Resolved function pointers (accessible by UnityHook)
    struct ResolvedAPIs {
        // IL2CPP
        il2cpp_domain_get_t                    il2cpp_domain_get = nullptr;
        il2cpp_thread_attach_t                 il2cpp_thread_attach = nullptr;
        il2cpp_domain_get_assemblies_t         il2cpp_domain_get_assemblies = nullptr;
        il2cpp_assembly_get_image_t            il2cpp_assembly_get_image = nullptr;
        il2cpp_class_from_name_t               il2cpp_class_from_name = nullptr;
        il2cpp_class_get_methods_t             il2cpp_class_get_methods = nullptr;
        il2cpp_class_get_method_from_name_t    il2cpp_class_get_method_from_name = nullptr;
        il2cpp_method_get_name_t               il2cpp_method_get_name = nullptr;
        il2cpp_image_get_name_t                il2cpp_image_get_name = nullptr;
        il2cpp_image_get_class_count_t         il2cpp_image_get_class_count = nullptr;
        il2cpp_image_get_class_t               il2cpp_image_get_class = nullptr;
        il2cpp_class_get_name_t                il2cpp_class_get_name = nullptr;
        il2cpp_class_get_namespace_t           il2cpp_class_get_namespace = nullptr;

        // Mono
        mono_domain_get_t                      mono_domain_get = nullptr;
        mono_thread_attach_t                   mono_thread_attach = nullptr;
        mono_domain_assembly_open_t            mono_domain_assembly_open = nullptr;
        mono_assembly_get_image_t              mono_assembly_get_image = nullptr;
        mono_class_from_name_t                 mono_class_from_name = nullptr;
        mono_class_get_method_from_name_t      mono_class_get_method_from_name = nullptr;
        mono_compile_method_t                  mono_compile_method = nullptr;
    };

    const ResolvedAPIs& GetAPIs() const { return m_apis; }

    // Resolved camera method pointers (for UnityHook to detour)
    void* GetCameraSetWorldToCameraMatrix() const { return m_cameraSetWorldToCameraMatrix; }
    void* GetCameraSetProjectionMatrix() const { return m_cameraSetProjectionMatrix; }
    void* GetCameraGetMain() const { return m_cameraGetMain; }

private:
    UnityScanner() = default;
    ~UnityScanner() = default;

    bool ResolveIL2CPPApis();
    bool ResolveMonoApis();
    bool FindCameraMethods_IL2CPP();
    bool FindCameraMethods_Mono();

    bool m_isUnity = false;
    bool m_initialized = false;
    UnityBackend m_backend = UnityBackend::Unknown;

    HMODULE m_monoModule = nullptr;
    HMODULE m_il2cppModule = nullptr;

    ResolvedAPIs m_apis;

    // Resolved camera method addresses
    void* m_cameraSetWorldToCameraMatrix = nullptr;
    void* m_cameraSetProjectionMatrix = nullptr;
    void* m_cameraGetMain = nullptr;
};

} // namespace engine_scanners
} // namespace vrinject
