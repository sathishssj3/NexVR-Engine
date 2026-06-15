#include "unity_scanner.h"
#include "../logger.h"
#include "../memory_scanner.h"

namespace vrinject {
namespace engine_scanners {

// ============================================================================
// Helper macro to resolve a function from a DLL
// ============================================================================
#define RESOLVE_API(module, funcName, target) \
    target = reinterpret_cast<decltype(target)>(GetProcAddress(module, #funcName)); \
    if (!target) { \
        LOG_WARN("UnityScanner: Failed to resolve " #funcName); \
    } else { \
        LOG_INFO("UnityScanner: Resolved " #funcName " at %p", target); \
    }

bool UnityScanner::Initialize() {
    if (m_initialized) return true;

    // Step 1: Detect which Unity backend is present
    m_il2cppModule = GetModuleHandleA("GameAssembly.dll");
    m_monoModule = GetModuleHandleA("mono-2.0-bdwgc.dll");
    
    // Some older Unity versions use different Mono DLL names
    if (!m_monoModule) {
        m_monoModule = GetModuleHandleA("mono.dll");
    }

    if (m_il2cppModule) {
        m_isUnity = true;
        m_backend = UnityBackend::IL2CPP;
        LOG_INFO("UnityScanner: Detected Unity Engine (IL2CPP backend) - GameAssembly.dll at %p", m_il2cppModule);
    } else if (m_monoModule) {
        m_isUnity = true;
        m_backend = UnityBackend::Mono;
        LOG_INFO("UnityScanner: Detected Unity Engine (Mono backend) - Module at %p", m_monoModule);
    } else {
        LOG_INFO("UnityScanner: No Unity runtime DLLs found. Not a Unity game.");
        m_initialized = true;
        return true;
    }

    // Step 2: Resolve the scripting API functions
    if (m_backend == UnityBackend::IL2CPP) {
        if (!ResolveIL2CPPApis()) {
            LOG_ERROR("UnityScanner: Failed to resolve critical IL2CPP APIs.");
            return false;
        }
    } else {
        if (!ResolveMonoApis()) {
            LOG_ERROR("UnityScanner: Failed to resolve critical Mono APIs.");
            return false;
        }
    }

    m_initialized = true;
    return true;
}

void UnityScanner::Shutdown() {
    m_initialized = false;
    m_isUnity = false;
    m_backend = UnityBackend::Unknown;
    m_monoModule = nullptr;
    m_il2cppModule = nullptr;
    m_cameraSetWorldToCameraMatrix = nullptr;
    m_cameraSetProjectionMatrix = nullptr;
    m_cameraGetMain = nullptr;
    m_apis = {};
}

// ============================================================================
// IL2CPP API Resolution
// ============================================================================
bool UnityScanner::ResolveIL2CPPApis() {
    LOG_INFO("UnityScanner: Resolving IL2CPP API exports from GameAssembly.dll...");

    // Core domain/thread functions
    RESOLVE_API(m_il2cppModule, il2cpp_domain_get, m_apis.il2cpp_domain_get);
    RESOLVE_API(m_il2cppModule, il2cpp_thread_attach, m_apis.il2cpp_thread_attach);
    RESOLVE_API(m_il2cppModule, il2cpp_domain_get_assemblies, m_apis.il2cpp_domain_get_assemblies);
    RESOLVE_API(m_il2cppModule, il2cpp_assembly_get_image, m_apis.il2cpp_assembly_get_image);

    // Type system functions
    RESOLVE_API(m_il2cppModule, il2cpp_class_from_name, m_apis.il2cpp_class_from_name);
    RESOLVE_API(m_il2cppModule, il2cpp_class_get_methods, m_apis.il2cpp_class_get_methods);
    RESOLVE_API(m_il2cppModule, il2cpp_class_get_method_from_name, m_apis.il2cpp_class_get_method_from_name);
    RESOLVE_API(m_il2cppModule, il2cpp_method_get_name, m_apis.il2cpp_method_get_name);

    // Image/class introspection
    RESOLVE_API(m_il2cppModule, il2cpp_image_get_name, m_apis.il2cpp_image_get_name);
    RESOLVE_API(m_il2cppModule, il2cpp_image_get_class_count, m_apis.il2cpp_image_get_class_count);
    RESOLVE_API(m_il2cppModule, il2cpp_image_get_class, m_apis.il2cpp_image_get_class);
    RESOLVE_API(m_il2cppModule, il2cpp_class_get_name, m_apis.il2cpp_class_get_name);
    RESOLVE_API(m_il2cppModule, il2cpp_class_get_namespace, m_apis.il2cpp_class_get_namespace);

    // Check minimum required APIs
    if (!m_apis.il2cpp_domain_get || !m_apis.il2cpp_thread_attach || 
        !m_apis.il2cpp_class_from_name || !m_apis.il2cpp_class_get_method_from_name) {
        LOG_ERROR("UnityScanner: Missing critical IL2CPP APIs.");
        return false;
    }

    LOG_INFO("UnityScanner: IL2CPP APIs resolved successfully.");
    return true;
}

// ============================================================================
// Mono API Resolution
// ============================================================================
bool UnityScanner::ResolveMonoApis() {
    LOG_INFO("UnityScanner: Resolving Mono API exports...");

    RESOLVE_API(m_monoModule, mono_domain_get, m_apis.mono_domain_get);
    RESOLVE_API(m_monoModule, mono_thread_attach, m_apis.mono_thread_attach);
    RESOLVE_API(m_monoModule, mono_domain_assembly_open, m_apis.mono_domain_assembly_open);
    RESOLVE_API(m_monoModule, mono_assembly_get_image, m_apis.mono_assembly_get_image);
    RESOLVE_API(m_monoModule, mono_class_from_name, m_apis.mono_class_from_name);
    RESOLVE_API(m_monoModule, mono_class_get_method_from_name, m_apis.mono_class_get_method_from_name);
    RESOLVE_API(m_monoModule, mono_compile_method, m_apis.mono_compile_method);

    if (!m_apis.mono_domain_get || !m_apis.mono_thread_attach ||
        !m_apis.mono_class_from_name || !m_apis.mono_class_get_method_from_name) {
        LOG_ERROR("UnityScanner: Missing critical Mono APIs.");
        return false;
    }

    LOG_INFO("UnityScanner: Mono APIs resolved successfully.");
    return true;
}

// ============================================================================
// Find Camera Methods via IL2CPP
// ============================================================================
bool UnityScanner::FindCameraMethods_IL2CPP() {
    LOG_INFO("UnityScanner: Searching for UnityEngine.Camera methods via IL2CPP...");

    // Attach our injector thread to the IL2CPP runtime
    void* domain = m_apis.il2cpp_domain_get();
    if (!domain) {
        LOG_ERROR("UnityScanner: il2cpp_domain_get returned null.");
        return false;
    }

    void* thread = m_apis.il2cpp_thread_attach(domain);
    if (!thread) {
        LOG_ERROR("UnityScanner: il2cpp_thread_attach failed.");
        return false;
    }

    LOG_INFO("UnityScanner: Attached to IL2CPP domain. Searching assemblies...");

    // Find the UnityEngine.CoreModule assembly
    size_t assemblyCount = 0;
    void** assemblies = static_cast<void**>(m_apis.il2cpp_domain_get_assemblies(domain, &assemblyCount));
    
    if (!assemblies || assemblyCount == 0) {
        LOG_ERROR("UnityScanner: No assemblies found in IL2CPP domain.");
        return false;
    }

    LOG_INFO("UnityScanner: Found %zu assemblies.", assemblyCount);

    void* coreModuleImage = nullptr;
    for (size_t i = 0; i < assemblyCount; i++) {
        void* image = m_apis.il2cpp_assembly_get_image(assemblies[i]);
        if (!image) continue;

        const char* imageName = m_apis.il2cpp_image_get_name(image);
        if (!imageName) continue;

        // Look for UnityEngine.CoreModule.dll (Unity 2017.1+) or UnityEngine.dll (older)
        if (strstr(imageName, "UnityEngine.CoreModule") || 
            (strcmp(imageName, "UnityEngine.dll") == 0)) {
            coreModuleImage = image;
            LOG_INFO("UnityScanner: Found core module: %s", imageName);
            break;
        }
    }

    if (!coreModuleImage) {
        LOG_ERROR("UnityScanner: Could not find UnityEngine.CoreModule.");
        return false;
    }

    // Find the Camera class
    void* cameraClass = m_apis.il2cpp_class_from_name(coreModuleImage, "UnityEngine", "Camera");
    if (!cameraClass) {
        LOG_ERROR("UnityScanner: Could not find UnityEngine.Camera class.");
        return false;
    }
    LOG_INFO("UnityScanner: Found UnityEngine.Camera class at %p", cameraClass);

    // Find Camera.get_main (static method, 0 params)
    void* getMainMethod = m_apis.il2cpp_class_get_method_from_name(cameraClass, "get_main", 0);
    if (getMainMethod) {
        // In IL2CPP, the MethodInfo contains the native method pointer at offset 0
        m_cameraGetMain = *reinterpret_cast<void**>(getMainMethod);
        LOG_INFO("UnityScanner: Found Camera.get_main at %p", m_cameraGetMain);
    }

    // Find Camera.set_worldToCameraMatrix (1 param: Matrix4x4)
    void* setWorldToCameraMethod = m_apis.il2cpp_class_get_method_from_name(cameraClass, "set_worldToCameraMatrix", 1);
    if (setWorldToCameraMethod) {
        m_cameraSetWorldToCameraMatrix = *reinterpret_cast<void**>(setWorldToCameraMethod);
        LOG_INFO("UnityScanner: Found Camera.set_worldToCameraMatrix at %p", m_cameraSetWorldToCameraMatrix);
    }

    // Find Camera.set_projectionMatrix (1 param: Matrix4x4)
    void* setProjectionMethod = m_apis.il2cpp_class_get_method_from_name(cameraClass, "set_projectionMatrix", 1);
    if (setProjectionMethod) {
        m_cameraSetProjectionMatrix = *reinterpret_cast<void**>(setProjectionMethod);
        LOG_INFO("UnityScanner: Found Camera.set_projectionMatrix at %p", m_cameraSetProjectionMatrix);
    }

    bool success = (m_cameraGetMain != nullptr);
    if (success) {
        LOG_INFO("UnityScanner: Successfully located Unity Camera methods!");
    } else {
        LOG_WARN("UnityScanner: Could not locate Camera.get_main. Unity camera hooking may be limited.");
    }

    return success;
}

// ============================================================================
// Find Camera Methods via Mono
// ============================================================================
bool UnityScanner::FindCameraMethods_Mono() {
    LOG_INFO("UnityScanner: Searching for UnityEngine.Camera methods via Mono...");

    void* domain = m_apis.mono_domain_get();
    if (!domain) {
        LOG_ERROR("UnityScanner: mono_domain_get returned null.");
        return false;
    }

    m_apis.mono_thread_attach(domain);

    // Open the UnityEngine assembly
    void* assembly = m_apis.mono_domain_assembly_open(domain, "UnityEngine");
    if (!assembly) {
        // Try alternate name for newer Unity versions
        assembly = m_apis.mono_domain_assembly_open(domain, "UnityEngine.CoreModule");
    }
    if (!assembly) {
        LOG_ERROR("UnityScanner: Could not open UnityEngine assembly in Mono.");
        return false;
    }

    void* image = m_apis.mono_assembly_get_image(assembly);
    if (!image) {
        LOG_ERROR("UnityScanner: mono_assembly_get_image failed.");
        return false;
    }

    // Find Camera class
    void* cameraClass = m_apis.mono_class_from_name(image, "UnityEngine", "Camera");
    if (!cameraClass) {
        LOG_ERROR("UnityScanner: Could not find UnityEngine.Camera in Mono.");
        return false;
    }
    LOG_INFO("UnityScanner: Found UnityEngine.Camera class (Mono) at %p", cameraClass);

    // Find Camera.get_main
    void* getMainMethod = m_apis.mono_class_get_method_from_name(cameraClass, "get_main", 0);
    if (getMainMethod && m_apis.mono_compile_method) {
        m_cameraGetMain = m_apis.mono_compile_method(getMainMethod);
        LOG_INFO("UnityScanner: Compiled Camera.get_main at %p", m_cameraGetMain);
    }

    // Find Camera.set_worldToCameraMatrix
    void* setWtcMethod = m_apis.mono_class_get_method_from_name(cameraClass, "set_worldToCameraMatrix", 1);
    if (setWtcMethod && m_apis.mono_compile_method) {
        m_cameraSetWorldToCameraMatrix = m_apis.mono_compile_method(setWtcMethod);
        LOG_INFO("UnityScanner: Compiled Camera.set_worldToCameraMatrix at %p", m_cameraSetWorldToCameraMatrix);
    }

    // Find Camera.set_projectionMatrix
    void* setProjMethod = m_apis.mono_class_get_method_from_name(cameraClass, "set_projectionMatrix", 1);
    if (setProjMethod && m_apis.mono_compile_method) {
        m_cameraSetProjectionMatrix = m_apis.mono_compile_method(setProjMethod);
        LOG_INFO("UnityScanner: Compiled Camera.set_projectionMatrix at %p", m_cameraSetProjectionMatrix);
    }

    return (m_cameraGetMain != nullptr);
}

// ============================================================================
// HookCamera: Locate camera methods, then let UnityHook install detours
// ============================================================================
bool UnityScanner::HookCamera() {
    if (!m_initialized || !m_isUnity) {
        LOG_WARN("UnityScanner::HookCamera called but scanner not ready.");
        return false;
    }

    bool found = false;
    if (m_backend == UnityBackend::IL2CPP) {
        found = FindCameraMethods_IL2CPP();
    } else if (m_backend == UnityBackend::Mono) {
        found = FindCameraMethods_Mono();
    }

    if (!found) {
        LOG_WARN("UnityScanner: Could not locate Camera methods. Falling back to Universal Mode.");
        return false;
    }

    // Also try to find the matrix setter methods via AOB signature scanning
    // as a fallback if the API-based approach didn't find them
    if (!m_cameraSetWorldToCameraMatrix || !m_cameraSetProjectionMatrix) {
        LOG_INFO("UnityScanner: Attempting AOB scan for camera matrix setters in GameAssembly.dll...");
        auto& memScanner = MemoryScanner::Get();
        if (!memScanner.Initialize()) {
            LOG_WARN("UnityScanner: MemoryScanner init failed for AOB fallback.");
        } else {
            const char* moduleName = (m_backend == UnityBackend::IL2CPP) ? "GameAssembly.dll" : "";
            
            // Common IL2CPP signature for Camera::set_worldToCameraMatrix
            const std::string sig_SetWTC = "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 8B F2 48 8B F9 48 8B 0D";
            if (!m_cameraSetWorldToCameraMatrix) {
                uint8_t* result = memScanner.ScanSignature(sig_SetWTC, moduleName);
                if (result) {
                    m_cameraSetWorldToCameraMatrix = result;
                    LOG_INFO("UnityScanner: Found set_worldToCameraMatrix via AOB at %p", result);
                }
            }

            const std::string sig_SetProj = "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 41 56 41 57 48 83 EC ? 48 8B F2";
            if (!m_cameraSetProjectionMatrix) {
                uint8_t* result = memScanner.ScanSignature(sig_SetProj, moduleName);
                if (result) {
                    m_cameraSetProjectionMatrix = result;
                    LOG_INFO("UnityScanner: Found set_projectionMatrix via AOB at %p", result);
                }
            }
        }
    }

    LOG_INFO("UnityScanner: Camera method resolution complete. Ready for UnityHook.");
    return true;
}

} // namespace engine_scanners
} // namespace vrinject
