cmake_minimum_required(VERSION 3.25)
project(VRInject VERSION 0.1.0 LANGUAGES C CXX)



set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
include(cmake/CompilerFlags.cmake)
include(cmake/Dependencies.cmake)
include(cmake/Shaders.cmake)

file(GLOB_RECURSE VRINJECT_CORE_SRCS "src/core/*.cpp")
file(GLOB_RECURSE VRINJECT_HEURISTICS_SRCS "src/heuristics/*.cpp")
file(GLOB_RECURSE VRINJECT_RENDERING_SRCS "src/rendering/*.cpp")
file(GLOB_RECURSE VRINJECT_OPENXR_SRCS "src/openxr/*.cpp")
file(GLOB_RECURSE VRINJECT_VR_SRCS "src/vr/*.cpp")
file(GLOB_RECURSE VRINJECT_HOOKS_SRCS "src/hooks/*.cpp")
file(GLOB_RECURSE VRINJECT_AI_SRCS "src/ai/*.cpp")
file(GLOB_RECURSE VRINJECT_RUNTIME_SRCS "src/runtime/*.cpp")
file(GLOB_RECURSE VRINJECT_ADAPTIVE_SRCS "src/adaptive/*.cpp")
file(GLOB_RECURSE VRINJECT_DIAGNOSTICS_SRCS "src/diagnostics/*.cpp")
file(GLOB_RECURSE VRINJECT_CONFIG_SRCS "src/config/*.cpp")
file(GLOB_RECURSE VRINJECT_MEMORY_SCANNER_SRCS "src/memory_scanner/*.cpp")
file(GLOB_RECURSE VRINJECT_UPDATER_SRCS "src/updater/*.cpp")
file(GLOB_RECURSE VRINJECT_PLUGINS_SRCS "src/plugins/*.cpp")
file(GLOB_RECURSE VRINJECT_UI_SRCS "src/ui/*.cpp")
file(GLOB_RECURSE VRINJECT_RELEASE_SRCS "src/release/*.cpp")
file(GLOB_RECURSE VRINJECT_VALIDATION_SRCS "src/validation/*.cpp")
file(GLOB_RECURSE VRINJECT_FEEDBACK_SRCS "src/feedback/*.cpp")
file(GLOB_RECURSE VRINJECT_MEMORY_SCANNER_SRCS "src/memory_scanner/*.cpp")

add_library(vrinject SHARED
    ${VRINJECT_CORE_SRCS}
    ${VRINJECT_HEURISTICS_SRCS}
    ${VRINJECT_RENDERING_SRCS}
    ${VRINJECT_OPENXR_SRCS}
    ${VRINJECT_VR_SRCS}
    ${VRINJECT_HOOKS_SRCS}
    ${VRINJECT_AI_SRCS}
    ${VRINJECT_RUNTIME_SRCS}
    ${VRINJECT_ADAPTIVE_SRCS}
    ${VRINJECT_DIAGNOSTICS_SRCS}
    ${VRINJECT_CONFIG_SRCS}
    ${VRINJECT_MEMORY_SCANNER_SRCS}
    ${VRINJECT_UPDATER_SRCS}
    ${VRINJECT_PLUGINS_SRCS}
    ${VRINJECT_UI_SRCS}
    ${VRINJECT_RELEASE_SRCS}
    ${VRINJECT_VALIDATION_SRCS}
    ${VRINJECT_FEEDBACK_SRCS}
    ${VRINJECT_MEMORY_SCANNER_SRCS}
    src/dllmain.cpp
    ${MINHOOK_SOURCES}
    ${IMGUI_SOURCES}
)

# ---------------------------------------------------------------------------
# Proxy DLLs (dxgi.dll & d3d11.dll)
# ---------------------------------------------------------------------------
add_library(dxgi_proxy SHARED
    src/proxy/dxgi_proxy.cpp
)
# NOTE: these proxies are deliberately named dxgi.dll / d3d11.dll so they can be
# dropped next to a game executable and take load precedence over the system
# DLLs. That same precedence is harmful inside bin/, where it shadowed the real
# system DLLs for every test executable and made them fail to start with
# 0xC000007B (STATUS_INVALID_IMAGE_FORMAT). They are emitted into bin/proxy/
# instead; deployment copies them next to the target game, not next to tests.
set_target_properties(dxgi_proxy PROPERTIES
    OUTPUT_NAME "dxgi"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/proxy
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/proxy
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/proxy
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/bin/proxy
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/bin/proxy
)
target_link_libraries(dxgi_proxy PRIVATE user32)

add_library(d3d11_proxy SHARED
    src/proxy/d3d11_proxy.cpp
)
set_target_properties(d3d11_proxy PROPERTIES
    OUTPUT_NAME "d3d11"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/proxy
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/proxy
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/proxy
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/bin/proxy
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/bin/proxy
)
target_link_libraries(d3d11_proxy PRIVATE user32)

# S1.1: Code-sign d3d11.dll
if (DEFINED ENV{SIGN_CERT_PATH})
    add_custom_command(TARGET d3d11_proxy POST_BUILD
      COMMAND signtool.exe sign
        /f "$ENV{SIGN_CERT_PATH}"
        /p "$ENV{SIGN_CERT_PASS}"
        /tr http://timestamp.digicert.com
        /td sha256 /fd sha256
        "$<TARGET_FILE:d3d11_proxy>"
      COMMENT "Signing d3d11.dll")
endif()

# S1.1: Code-sign dxgi.dll
if (DEFINED ENV{SIGN_CERT_PATH})
    add_custom_command(TARGET dxgi_proxy POST_BUILD
      COMMAND signtool.exe sign
        /f "$ENV{SIGN_CERT_PATH}"
        /p "$ENV{SIGN_CERT_PASS}"
        /tr http://timestamp.digicert.com
        /td sha256 /fd sha256
        "$<TARGET_FILE:dxgi_proxy>"
      COMMENT "Signing dxgi.dll")
endif()


target_include_directories(vrinject PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
    ${MINHOOK_INCLUDE_DIR}
    ${ONNXRUNTIME_INCLUDE_DIR}
    ${OPENXR_INCLUDE_DIR}
    ${JSON_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${SHADER_OUTPUT_DIR}
)

target_link_directories(vrinject PRIVATE ${ONNXRUNTIME_LIB_DIR})

target_link_libraries(vrinject PRIVATE
    d3d11
    d3d12
    dxgi
    d3dcompiler
    dxguid
    onnxruntime
    openxr_loader
    xinput
    Vulkan::Vulkan
    bcrypt
    shell32
)

# Copy native AI runtime dependencies next to the injected DLL.
add_custom_command(TARGET vrinject POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${onnxruntime_SOURCE_DIR}/runtimes/win-x64/native/onnxruntime.dll"
        $<TARGET_FILE_DIR:vrinject>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${directml_SOURCE_DIR}/bin/x64-win/DirectML.dll"
        $<TARGET_FILE_DIR:vrinject>
)

# S1.1: Code-sign vrinject.dll
if (DEFINED ENV{SIGN_CERT_PATH})
    add_custom_command(TARGET vrinject POST_BUILD
      COMMAND signtool.exe sign
        /f "$ENV{SIGN_CERT_PATH}"
        /p "$ENV{SIGN_CERT_PASS}"
        /tr http://timestamp.digicert.com
        /td sha256 /fd sha256
        "$<TARGET_FILE:vrinject>"
      COMMENT "Signing vrinject.dll")
endif()

# S1.4: Generate DLL hash header
file(WRITE "${CMAKE_BINARY_DIR}/generate_hash.ps1"
"param($DllPath, $OutPath)
$stream = [System.IO.File]::OpenRead($DllPath)
$sha256 = [System.Security.Cryptography.SHA256]::Create()
$hashBytes = $sha256.ComputeHash($stream)
$stream.Close()
$hashString = [System.BitConverter]::ToString($hashBytes).Replace('-', '')
Set-Content -Path $OutPath -Value \"constexpr wchar_t EXPECTED_DLL_HASH[] = L`\"$hashString`\";\"
")

add_custom_command(TARGET vrinject POST_BUILD
  COMMAND powershell -NonInteractive -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_BINARY_DIR}/generate_hash.ps1" "$<TARGET_FILE:vrinject>" "${CMAKE_BINARY_DIR}/expected_hash.h"
  COMMENT "Generating DLL hash header")

# Make sure shaders are compiled before the DLL is built.
add_dependencies(vrinject compile_shaders)





# ---------------------------------------------------------------------------
# vr-inject-cli - injection CLI
# ---------------------------------------------------------------------------
add_executable(vr-inject-cli src/injector/main.cpp)
add_dependencies(vr-inject-cli vrinject)
add_custom_target(generate_expected_hash DEPENDS "${CMAKE_BINARY_DIR}/expected_hash.h")
add_dependencies(vr-inject-cli generate_expected_hash)
set_target_properties(vr-inject-cli PROPERTIES LINK_FLAGS "/MANIFESTUAC:\"level='asInvoker' uiAccess='false'\" /SUBSYSTEM:CONSOLE")
target_include_directories(vr-inject-cli PRIVATE ${CMAKE_BINARY_DIR})
target_link_libraries(vr-inject-cli PRIVATE shlwapi bcrypt psapi)
add_dependencies(vr-inject-cli vrinject)

# S1.1: Code-sign vr-inject-cli.exe
if (DEFINED ENV{SIGN_CERT_PATH})
    add_custom_command(TARGET vr-inject-cli POST_BUILD
      COMMAND signtool.exe sign
        /f "$ENV{SIGN_CERT_PATH}"
        /p "$ENV{SIGN_CERT_PASS}"
        /tr http://timestamp.digicert.com
        /td sha256 /fd sha256
        "$<TARGET_FILE:vr-inject-cli>"
      COMMENT "Signing vr-inject-cli.exe")
endif()

add_executable(test_xr src/test_xr.cpp)
target_include_directories(test_xr PRIVATE ${OPENXR_INCLUDE_DIR})
target_link_libraries(test_xr PRIVATE openxr_loader)

# ---------------------------------------------------------------------------
# vk_test_app - target app for testing Vulkan injection
# ---------------------------------------------------------------------------
add_executable(vk_test_app src/test_apps/vk_test_app.cpp)
target_link_libraries(vk_test_app PRIVATE Vulkan::Vulkan)


add_library(NexVRCore STATIC 
    ${VRINJECT_CORE_SRCS}
    ${VRINJECT_HEURISTICS_SRCS}
    ${VRINJECT_RENDERING_SRCS}
    ${VRINJECT_OPENXR_SRCS}
    ${VRINJECT_VR_SRCS}
    ${VRINJECT_HOOKS_SRCS}
    ${VRINJECT_AI_SRCS}
    ${VRINJECT_RUNTIME_SRCS}
    ${VRINJECT_DIAGNOSTICS_SRCS}
    ${VRINJECT_CONFIG_SRCS}
    ${VRINJECT_MEMORY_SCANNER_SRCS}
    ${VRINJECT_ADAPTIVE_SRCS}
    ${VRINJECT_PLUGINS_SRCS}
    ${VRINJECT_UPDATER_SRCS}
    ${VRINJECT_RELEASE_SRCS}
    ${VRINJECT_VALIDATION_SRCS}
    ${VRINJECT_FEEDBACK_SRCS}
    ${VRINJECT_MEMORY_SCANNER_SRCS}
    ${MINHOOK_SOURCES}
    ${IMGUI_SOURCES}
)

target_include_directories(NexVRCore PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
    ${MINHOOK_INCLUDE_DIR}
    ${ONNXRUNTIME_INCLUDE_DIR}
    ${OPENXR_INCLUDE_DIR}
    ${JSON_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${SHADER_OUTPUT_DIR}
)
target_link_libraries(NexVRCore PRIVATE Vulkan::Vulkan openxr_loader d3d11 d3d12 dxgi)
add_dependencies(NexVRCore copy_shaders_for_dx12)

add_executable(test_openxr_vulkan_swapchain tests/test_openxr_vulkan_swapchain.cpp)
target_include_directories(test_openxr_vulkan_swapchain PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src ${GTEST_INCLUDE_DIRS} ${OPENXR_INCLUDE_DIR})
target_link_libraries(test_openxr_vulkan_swapchain PRIVATE NexVRCore Vulkan::Vulkan openxr_loader)

add_executable(test_openxr_harness tests/harness/test_openxr_harness.cpp tests/harness/mock_openxr_runtime.cpp)
target_include_directories(test_openxr_harness PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR} ${GTEST_INCLUDE_DIRS} ${OPENXR_INCLUDE_DIR} ${MINHOOK_INCLUDE_DIR} ${Vulkan_INCLUDE_DIRS})
target_link_libraries(test_openxr_harness PRIVATE NexVRCore d3d11 gtest_main)
add_test(NAME test_openxr_harness COMMAND test_openxr_harness)

include(cmake/Testing.cmake)

