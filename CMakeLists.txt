cmake_minimum_required(VERSION 3.25)
project(VRInject VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_POLICY_DEFAULT_CMP0169 OLD)
set(CMAKE_POLICY_DEFAULT_CMP0148 OLD)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Global settings
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Security & Hardening compile/link options
if(MSVC)
    add_compile_options(/GS /guard:cf /EHa)
    add_link_options(/DYNAMICBASE /NXCOMPAT /GUARD:CF)
else()
    add_compile_options(-fstack-protector-strong -D_FORTIFY_SOURCE=2)
    add_link_options(-Wl,-z,now -Wl,-z,relro)
endif()

# Force all build artifacts into a single bin/ directory so the injector
# can locate vrinject.dll next to itself at runtime.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Per-configuration overrides (Multi-config generators like VS ignore the
# above without these).
foreach(CFG IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER ${CFG} CFG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_UPPER} ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_UPPER} ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_UPPER} ${CMAKE_BINARY_DIR}/bin)
endforeach()

# ---------------------------------------------------------------------------
# Dependencies – MinHook (inline hooking library)
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    minhook
    URL https://github.com/TsudaKageyu/minhook/archive/refs/tags/v1.3.3.zip
)
FetchContent_GetProperties(minhook)
if(NOT minhook_POPULATED)
    FetchContent_Populate(minhook)
endif()
set(MINHOOK_INCLUDE_DIR ${minhook_SOURCE_DIR}/include)
set(MINHOOK_SOURCES
    ${minhook_SOURCE_DIR}/src/buffer.c
    ${minhook_SOURCE_DIR}/src/hook.c
    ${minhook_SOURCE_DIR}/src/trampoline.c
    ${minhook_SOURCE_DIR}/src/hde/hde32.c
    ${minhook_SOURCE_DIR}/src/hde/hde64.c
)


# ---------------------------------------------------------------------------
# Dependencies – ONNX Runtime (DirectML)
# ---------------------------------------------------------------------------
# We download the pre-compiled ONNX Runtime DirectML release for Windows x64 via NuGet.
FetchContent_Declare(
    onnxruntime
    URL https://www.nuget.org/api/v2/package/Microsoft.ML.OnnxRuntime.DirectML/1.16.3
)
FetchContent_GetProperties(onnxruntime)
if(NOT onnxruntime_POPULATED)
    FetchContent_Populate(onnxruntime)
endif()
set(ONNXRUNTIME_INCLUDE_DIR ${onnxruntime_SOURCE_DIR}/build/native/include)
set(ONNXRUNTIME_LIB_DIR ${onnxruntime_SOURCE_DIR}/runtimes/win-x64/native)

# We also need the DirectML redist DLL
FetchContent_Declare(
    directml
    URL https://www.nuget.org/api/v2/package/Microsoft.AI.DirectML/1.13.1
)
FetchContent_GetProperties(directml)
if(NOT directml_POPULATED)
    FetchContent_Populate(directml)
endif()

# ---------------------------------------------------------------------------
# Dependencies – Vulkan SDK
# ---------------------------------------------------------------------------
find_package(Vulkan REQUIRED)

# ---------------------------------------------------------------------------
# Dependencies – OpenXR SDK
# ---------------------------------------------------------------------------
FetchContent_Declare(
    openxr
    URL https://github.com/KhronosGroup/OpenXR-SDK/archive/refs/tags/release-1.0.34.zip
)
FetchContent_GetProperties(openxr)
if(NOT openxr_POPULATED)
    FetchContent_Populate(openxr)
endif()
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_CONFORMANCE OFF CACHE BOOL "" FORCE)
set(BUILD_LOADER ON CACHE BOOL "" FORCE)
set(BUILD_WITH_SHARED_CRT ON CACHE BOOL "" FORCE)
add_subdirectory(${openxr_SOURCE_DIR} ${openxr_BINARY_DIR} EXCLUDE_FROM_ALL)
set(OPENXR_INCLUDE_DIR ${openxr_SOURCE_DIR}/include)

# ---------------------------------------------------------------------------
# Dependencies – nlohmann/json
# ---------------------------------------------------------------------------
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_GetProperties(nlohmann_json)
if(NOT nlohmann_json_POPULATED)
    FetchContent_Populate(nlohmann_json)
endif()
set(JSON_INCLUDE_DIR ${nlohmann_json_SOURCE_DIR}/include)

# ---------------------------------------------------------------------------
# Dependencies - GoogleTest
# ---------------------------------------------------------------------------
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
    FetchContent_Populate(googletest)
    add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

# ---------------------------------------------------------------------------
# Dependencies – Dear ImGui
# ---------------------------------------------------------------------------
FetchContent_Declare(
    imgui
    URL https://github.com/ocornut/imgui/archive/refs/tags/v1.90.5.zip
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()
set(IMGUI_INCLUDE_DIR ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)


# ---------------------------------------------------------------------------
# HLSL shader compilation
# ---------------------------------------------------------------------------
# Locate fxc.exe from the Windows SDK.
find_program(FXC_EXECUTABLE fxc
    HINTS
        "$ENV{WindowsSdkDir}/bin/$ENV{WindowsSDKVersion}/x64"
        "$ENV{WindowsSdkDir}/bin/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22000.0/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.19041.0/x64"
)

if(NOT FXC_EXECUTABLE)
    message(WARNING "fxc.exe not found – HLSL shaders will NOT be compiled. "
                    "Install the Windows SDK or set FXC_EXECUTABLE manually.")
endif()

find_program(DXC_EXECUTABLE dxc
    HINTS
        "$ENV{WindowsSdkDir}/bin/$ENV{WindowsSDKVersion}/x64"
        "$ENV{WindowsSdkDir}/bin/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22000.0/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.19041.0/x64"
)
if(NOT DXC_EXECUTABLE)
    message(WARNING "dxc.exe not found. DX12 shaders will not be recompiled.")
endif()

set(SHADER_SOURCE_DIR ${CMAKE_SOURCE_DIR}/shaders)
set(SHADER_OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin/vrinject_shaders)

set(SHADER_SOURCES
    ${SHADER_SOURCE_DIR}/stereo_warp.hlsl
    ${SHADER_SOURCE_DIR}/stereo_resolve.hlsl
    ${SHADER_SOURCE_DIR}/disocclusion_fill.hlsl
    ${SHADER_SOURCE_DIR}/bilateral_blur.hlsl
    ${SHADER_SOURCE_DIR}/bilateral_blend.hlsl
    ${SHADER_SOURCE_DIR}/comfort_guard_analysis.hlsl
    ${SHADER_SOURCE_DIR}/asw_shader.hlsl
    ${SHADER_SOURCE_DIR}/depth_reprojection.hlsl
    ${SHADER_SOURCE_DIR}/tonemap.hlsl
    ${SHADER_SOURCE_DIR}/stereo_reprojection.hlsl
)

# Create output directory for compiled shaders.
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

# Copy the raw .hlsl files to the output directory so the launcher can deploy them for dynamic compilation
foreach(shader_src ${SHADER_SOURCES})
    get_filename_component(shader_name ${shader_src} NAME)
    configure_file(${shader_src} ${SHADER_OUTPUT_DIR}/${shader_name} COPYONLY)
endforeach()

# Copy Vulkan shaders
file(COPY ${CMAKE_SOURCE_DIR}/shaders/vulkan DESTINATION ${SHADER_OUTPUT_DIR})

set(SHADER_OUTPUTS "")

if(FXC_EXECUTABLE)
    foreach(SHADER_SRC IN LISTS SHADER_SOURCES)
        get_filename_component(SHADER_NAME ${SHADER_SRC} NAME_WE)
        set(SHADER_HDR ${SHADER_OUTPUT_DIR}/${SHADER_NAME}_cs_dx11.h)
        list(APPEND SHADER_OUTPUTS ${SHADER_HDR})

        add_custom_command(
            OUTPUT  ${SHADER_HDR}
            COMMAND ${FXC_EXECUTABLE}
                    /T cs_5_0
                    /E CSMain
                    /O2
                    /Fh ${SHADER_HDR}
                    /Vn g_${SHADER_NAME}_DX11
                    ${SHADER_SRC}
            DEPENDS ${SHADER_SRC}
            COMMENT "Compiling HLSL compute shader: ${SHADER_NAME}.hlsl -> ${SHADER_NAME}_cs_dx11.h"
            VERBATIM
        )

        set(SHADER_HDR_DX12 ${SHADER_OUTPUT_DIR}/${SHADER_NAME}_cs_dx12.h)
        list(APPEND SHADER_OUTPUTS ${SHADER_HDR_DX12})

        if(DXC_EXECUTABLE)
            add_custom_command(
                OUTPUT  ${SHADER_HDR_DX12}
                COMMAND ${DXC_EXECUTABLE}
                        -T cs_6_0
                        -E CSMain
                        -O3
                        -Fh ${SHADER_HDR_DX12}
                        -Vn g_${SHADER_NAME}_DX12
                        ${SHADER_SRC}
                DEPENDS ${SHADER_SRC}
                COMMENT "Compiling HLSL compute shader to DXIL: ${SHADER_NAME}.hlsl -> ${SHADER_NAME}_cs_dx12.h"
                VERBATIM
            )

            # Vulkan SPIR-V shader compilation using glslc (from Vulkan SDK)
            if (Vulkan_GLSLC_EXECUTABLE)
                set(SHADER_HDR_VK ${SHADER_OUTPUT_DIR}/${SHADER_NAME}_cs_vk.h)
                list(APPEND SHADER_OUTPUTS ${SHADER_HDR_VK})

                add_custom_command(
                    OUTPUT  ${SHADER_HDR_VK}
                    COMMAND ${Vulkan_GLSLC_EXECUTABLE}
                            -fshader-stage=compute
                            -x hlsl
                            -fentry-point=CSMain
                            -O
                            -mfmt=c
                            -o ${SHADER_HDR_VK}
                            ${SHADER_SRC}
                    DEPENDS ${SHADER_SRC}
                    COMMENT "Compiling HLSL compute shader to SPIR-V: ${SHADER_NAME}.hlsl -> ${SHADER_NAME}_cs_vk.h"
                    VERBATIM
                )
            endif()
        endif()
    endforeach()
endif()

# A convenience target so shaders rebuild when sources change.
add_custom_target(compile_shaders ALL DEPENDS ${SHADER_OUTPUTS})

file(GLOB_RECURSE VRINJECT_CORE_SRCS "src/core/*.cpp")
file(GLOB_RECURSE VRINJECT_RENDERING_SRCS "src/rendering/*.cpp")
file(GLOB_RECURSE VRINJECT_OPENXR_SRCS "src/openxr/*.cpp")
file(GLOB_RECURSE VRINJECT_VR_SRCS "src/vr/*.cpp")
file(GLOB_RECURSE VRINJECT_HOOKS_SRCS "src/hooks/*.cpp")
file(GLOB_RECURSE VRINJECT_AI_SRCS "src/ai/*.cpp")
file(GLOB_RECURSE VRINJECT_RUNTIME_SRCS "src/runtime/*.cpp")
file(GLOB_RECURSE VRINJECT_ADAPTIVE_SRCS "src/adaptive/*.cpp")
file(GLOB_RECURSE VRINJECT_DIAGNOSTICS_SRCS "src/diagnostics/*.cpp")
file(GLOB_RECURSE VRINJECT_CONFIG_SRCS "src/config/*.cpp")
file(GLOB_RECURSE VRINJECT_UPDATER_SRCS "src/updater/*.cpp")
file(GLOB_RECURSE VRINJECT_PLUGINS_SRCS "src/plugins/*.cpp")
file(GLOB_RECURSE VRINJECT_UI_SRCS "src/ui/*.cpp")
file(GLOB_RECURSE VRINJECT_RELEASE_SRCS "src/release/*.cpp")
file(GLOB_RECURSE VRINJECT_VALIDATION_SRCS "src/validation/*.cpp")
file(GLOB_RECURSE VRINJECT_FEEDBACK_SRCS "src/feedback/*.cpp")

add_library(vrinject SHARED
    ${VRINJECT_CORE_SRCS}
    ${VRINJECT_RENDERING_SRCS}
    ${VRINJECT_OPENXR_SRCS}
    ${VRINJECT_VR_SRCS}
    ${VRINJECT_HOOKS_SRCS}
    ${VRINJECT_AI_SRCS}
    ${VRINJECT_RUNTIME_SRCS}
    ${VRINJECT_ADAPTIVE_SRCS}
    ${VRINJECT_DIAGNOSTICS_SRCS}
    ${VRINJECT_CONFIG_SRCS}
    ${VRINJECT_UPDATER_SRCS}
    ${VRINJECT_PLUGINS_SRCS}
    ${VRINJECT_UI_SRCS}
    ${VRINJECT_RELEASE_SRCS}
    ${VRINJECT_VALIDATION_SRCS}
    ${VRINJECT_FEEDBACK_SRCS}
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


# S1.1: Code-sign dxgi.dll
# if (DEFINED ENV{SIGN_CERT_PATH})
#     add_custom_command(TARGET dxgi_proxy POST_BUILD
#       COMMAND signtool.exe sign
#         /f "$ENV{SIGN_CERT_PATH}"
#         /p "$ENV{SIGN_CERT_PASS}"
#         /tr http://timestamp.digicert.com
#         /td sha256 /fd sha256
#         "$<TARGET_FILE:dxgi_proxy>"
#       COMMENT "Signing dxgi.dll")
# endif()

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
# if (DEFINED ENV{SIGN_CERT_PATH})
#     add_custom_command(TARGET vrinject POST_BUILD
#       COMMAND signtool.exe sign
#         /f "$ENV{SIGN_CERT_PATH}"
#         /p "$ENV{SIGN_CERT_PASS}"
#         /tr http://timestamp.digicert.com
#         /td sha256 /fd sha256
#         "$<TARGET_FILE:vrinject>"
#       COMMENT "Signing vrinject.dll")
# endif()

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
# dummy_app - target app for testing injection
# ---------------------------------------------------------------------------
add_executable(dummy_app WIN32 tools/dummy_d3d11_app.cpp)

# ---------------------------------------------------------------------------
# vr-inject-cli - injection CLI
# ---------------------------------------------------------------------------
add_executable(vr-inject-cli src/injector/main.cpp)
add_dependencies(vr-inject-cli vrinject)
add_custom_target(generate_expected_hash DEPENDS "${CMAKE_BINARY_DIR}/expected_hash.h")
add_dependencies(vr-inject-cli generate_expected_hash)
set_target_properties(vr-inject-cli PROPERTIES LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\" /SUBSYSTEM:CONSOLE")
target_include_directories(vr-inject-cli PRIVATE ${CMAKE_BINARY_DIR})
target_link_libraries(vr-inject-cli PRIVATE shlwapi bcrypt psapi)
add_dependencies(vr-inject-cli vrinject)

# S1.1: Code-sign vr-inject-cli.exe
# if (DEFINED ENV{SIGN_CERT_PATH})
#     add_custom_command(TARGET vr-inject-cli POST_BUILD
#       COMMAND signtool.exe sign
#         /f "$ENV{SIGN_CERT_PATH}"
#         /p "$ENV{SIGN_CERT_PASS}"
#         /tr http://timestamp.digicert.com
#         /td sha256 /fd sha256
#         "$<TARGET_FILE:vr-inject-cli>"
#       COMMENT "Signing vr-inject-cli.exe")
# endif()

target_link_libraries(dummy_app PRIVATE
    d3d11
    user32
)

# ---------------------------------------------------------------------------
# test_app - testing vrinject DLL dynamically
# ---------------------------------------------------------------------------
add_executable(test_app WIN32 test_app/test_app.cpp)
target_link_libraries(test_app PRIVATE d3d11 dxgi user32)

# tests/e2e_test_app.cpp is currently an empty (0-byte) placeholder, so it has no
# entry point and fails to link (unresolved WinMain). Build it only once it has
# real content, rather than carrying a permanently broken target.
file(SIZE ${CMAKE_SOURCE_DIR}/tests/e2e_test_app.cpp E2E_TEST_APP_SIZE)
if(E2E_TEST_APP_SIZE GREATER 0)
    add_executable(e2e_test_app WIN32 tests/e2e_test_app.cpp)
    target_link_libraries(e2e_test_app PRIVATE d3d11 dxgi user32)
else()
    message(STATUS "Skipping e2e_test_app: tests/e2e_test_app.cpp is empty")
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
    ${VRINJECT_RENDERING_SRCS}
    ${VRINJECT_OPENXR_SRCS}
    ${VRINJECT_VR_SRCS}
    ${VRINJECT_HOOKS_SRCS}
    ${VRINJECT_AI_SRCS}
    ${VRINJECT_RUNTIME_SRCS}
    ${VRINJECT_DIAGNOSTICS_SRCS}
    ${VRINJECT_CONFIG_SRCS}
    ${VRINJECT_ADAPTIVE_SRCS}
    ${VRINJECT_PLUGINS_SRCS}
    ${VRINJECT_UPDATER_SRCS}
    ${VRINJECT_RELEASE_SRCS}
    ${VRINJECT_VALIDATION_SRCS}
    ${VRINJECT_FEEDBACK_SRCS}
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
add_dependencies(NexVRCore compile_shaders)

add_executable(test_openxr_vulkan_swapchain tests/test_openxr_vulkan_swapchain.cpp)
target_include_directories(test_openxr_vulkan_swapchain PRIVATE ${PROJECT_SOURCE_DIR} ${GTEST_INCLUDE_DIRS} ${OPENXR_INCLUDE_DIR})
target_link_libraries(test_openxr_vulkan_swapchain PRIVATE NexVRCore Vulkan::Vulkan openxr_loader)

enable_testing()

add_executable(test_compatibility_scorer tests/test_compatibility_scorer.cpp)
target_include_directories(test_compatibility_scorer PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(test_compatibility_scorer PRIVATE NexVRCore gtest_main)
add_test(NAME CompatibilityScorerTest COMMAND test_compatibility_scorer)

add_executable(test_engine_detector tests/test_engine_detector.cpp)
target_include_directories(test_engine_detector PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(test_engine_detector PRIVATE NexVRCore gtest_main)
add_test(NAME EngineDetectorTest COMMAND test_engine_detector)

# Returns TRUE only if <source> exists and is non-empty. Several test sources
# were restored as 0-byte shells; a target built from an empty translation unit
# always fails to link (unresolved main/WinMain), so those targets are skipped
# and reported at configure time instead of breaking the whole build.
function(nexvr_source_has_content result source)
    # The auto-registration loop passes absolute paths from file(GLOB), while the
    # explicit registrations pass repo-relative ones. Handle both.
    if(IS_ABSOLUTE "${source}")
        set(_full "${source}")
    else()
        set(_full "${CMAKE_SOURCE_DIR}/${source}")
    endif()
    set(${result} FALSE PARENT_SCOPE)
    if(EXISTS ${_full})
        file(SIZE ${_full} _sz)
        if(_sz GREATER 0)
            set(${result} TRUE PARENT_SCOPE)
        else()
            message(STATUS "Skipping target: ${source} is empty (0 bytes)")
        endif()
    else()
        message(STATUS "Skipping target: ${source} does not exist")
    endif()
endfunction()

function(add_nexvr_gtest target source)
    nexvr_source_has_content(_ok ${source})
    if(NOT _ok)
        return()
    endif()
    add_executable(${target} ${source})
    # gmock is linked (and its headers propagated) because several suites use
    # gmock matchers; OPENXR_INCLUDE_DIR is needed by the OpenXR lifecycle suites.
    target_include_directories(${target} PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR} ${Vulkan_INCLUDE_DIRS} ${OPENXR_INCLUDE_DIR})
    target_link_libraries(${target} PRIVATE NexVRCore gtest_main gmock Vulkan::Vulkan)
    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_nexvr_gtest(test_frame_timing_manager tests/test_frame_timing_manager.cpp)

add_executable(test_logger tests/test_logger.cpp)
target_include_directories(test_logger PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR})
target_link_libraries(test_logger PRIVATE gtest_main)
add_test(NAME test_logger COMMAND test_logger)

add_executable(test_adaptive_quality tests/test_adaptive_quality.cpp)
# PROJECT_SOURCE_DIR (not just /src) because this suite includes headers by
# repo-relative path, e.g. #include "src/rendering/adaptive_quality_controller.h".
target_include_directories(test_adaptive_quality PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src ${ONNXRUNTIME_INCLUDE_DIR})
target_link_directories(test_adaptive_quality PRIVATE ${ONNXRUNTIME_LIB_DIR})
target_link_libraries(test_adaptive_quality PRIVATE NexVRCore onnxruntime gtest_main)

add_test(NAME AdaptiveQualityTest COMMAND test_adaptive_quality)

nexvr_source_has_content(_ok tests/test_reconstruction_quality.cpp)
if(_ok)
    add_executable(test_reconstruction_quality tests/test_reconstruction_quality.cpp)
    target_include_directories(test_reconstruction_quality PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(test_reconstruction_quality PRIVATE NexVRCore)
    add_test(NAME ReconstructionQualityTest COMMAND test_reconstruction_quality)
endif()

nexvr_source_has_content(_ok tests/test_game_compatibility.cpp)
if(_ok)
    add_executable(test_game_compatibility tests/test_game_compatibility.cpp)
    target_include_directories(test_game_compatibility PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(test_game_compatibility PRIVATE NexVRCore gtest_main)
    add_test(NAME test_game_compatibility COMMAND test_game_compatibility)
endif()

nexvr_source_has_content(_ok tests/test_release_pipeline.cpp)
if(_ok)
    add_executable(test_release_pipeline tests/test_release_pipeline.cpp)
    target_include_directories(test_release_pipeline PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(test_release_pipeline PRIVATE NexVRCore gtest_main)
    add_test(NAME test_release_pipeline COMMAND test_release_pipeline)
endif()

add_nexvr_gtest(test_vulkan_gpu_profiler tests/test_vulkan_gpu_profiler.cpp)
add_executable(test_ai_directml tests/test_ai_directml.cpp)
target_include_directories(test_ai_directml PRIVATE ${PROJECT_SOURCE_DIR}/src ${ONNXRUNTIME_INCLUDE_DIR})
target_link_directories(test_ai_directml PRIVATE ${ONNXRUNTIME_LIB_DIR})
target_link_libraries(test_ai_directml PRIVATE NexVRCore onnxruntime)
add_test(NAME AiDirectMLTest COMMAND test_ai_directml)

add_executable(test_ai_ui_synthesizer tests/test_ai_ui_synthesizer.cpp)
target_include_directories(test_ai_ui_synthesizer PRIVATE ${PROJECT_SOURCE_DIR}/src ${ONNXRUNTIME_INCLUDE_DIR})
target_link_directories(test_ai_ui_synthesizer PRIVATE ${ONNXRUNTIME_LIB_DIR})
target_link_libraries(test_ai_ui_synthesizer PRIVATE NexVRCore onnxruntime)
add_test(NAME AiUISynthesizerTest COMMAND test_ai_ui_synthesizer)
add_nexvr_gtest(test_vulkan_async_scheduler tests/test_vulkan_async_scheduler.cpp)
add_nexvr_gtest(test_cross_backend_ai tests/test_cross_backend_ai.cpp)
add_nexvr_gtest(test_depth_pipeline tests/test_depth_pipeline.cpp)
add_nexvr_gtest(test_stereo_pipeline tests/test_stereo_pipeline.cpp)
if(TARGET test_depth_pipeline)
    target_include_directories(test_depth_pipeline PRIVATE ${PROJECT_SOURCE_DIR}/src ${ONNXRUNTIME_INCLUDE_DIR})
    target_link_directories(test_depth_pipeline PRIVATE ${ONNXRUNTIME_LIB_DIR})
    target_link_libraries(test_depth_pipeline PRIVATE onnxruntime)
endif()

add_nexvr_gtest(test_beta_validation tests/test_beta_validation.cpp)
if(TARGET test_beta_validation)
    target_compile_definitions(test_beta_validation PRIVATE NEXVR_VALIDATION_BUILD NEXVR_BUILD_CHANNEL_INTERNAL)
endif()

nexvr_source_has_content(_ok tests/test_runtime_capture.cpp)
if(_ok)
    add_executable(test_runtime_capture tests/test_runtime_capture.cpp)
    target_include_directories(test_runtime_capture PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(test_runtime_capture PRIVATE NexVRCore gtest_main)
    add_test(NAME test_runtime_capture COMMAND test_runtime_capture)
endif()

nexvr_source_has_content(_ok tests/test_runtime_integration.cpp)
if(_ok)
    add_executable(test_runtime_integration tests/test_runtime_integration.cpp)
    target_include_directories(test_runtime_integration PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(test_runtime_integration PRIVATE NexVRCore gtest_main)
    add_test(NAME test_runtime_integration COMMAND test_runtime_integration)
endif()



file(GLOB_RECURSE VR_SOURCES "src/vr/*.cpp")
file(GLOB_RECURSE VR_RENDERING_SOURCES "src/rendering/*.cpp")
nexvr_source_has_content(_ok tests/test_vr_runtime.cpp)
if(_ok)
    add_executable(test_vr_runtime tests/test_vr_runtime.cpp ${VR_SOURCES} ${VR_RENDERING_SOURCES})
    target_include_directories(test_vr_runtime PRIVATE src ${Vulkan_INCLUDE_DIRS} ${OPENXR_INCLUDE_DIR} ${SHADER_OUTPUT_DIR} ${IMGUI_INCLUDE_DIR})
    target_link_libraries(test_vr_runtime PRIVATE NexVRCore Vulkan::Vulkan openxr_loader)
    add_dependencies(test_vr_runtime compile_shaders)
endif()

nexvr_source_has_content(_ok tests/test_performance_pipeline.cpp)
if(_ok)
    add_executable(test_performance_pipeline tests/test_performance_pipeline.cpp)
    target_include_directories(test_performance_pipeline PRIVATE src ${Vulkan_INCLUDE_DIRS} ${OPENXR_INCLUDE_DIR} ${SHADER_OUTPUT_DIR} ${IMGUI_INCLUDE_DIR} ${ONNXRUNTIME_INCLUDE_DIR})
    target_link_directories(test_performance_pipeline PRIVATE ${ONNXRUNTIME_LIB_DIR})
    target_link_libraries(test_performance_pipeline PRIVATE NexVRCore Vulkan::Vulkan openxr_loader onnxruntime)
    add_dependencies(test_performance_pipeline compile_shaders)
endif()

if(TARGET test_stereo_pipeline)
target_include_directories(test_stereo_pipeline PRIVATE ${ONNXRUNTIME_INCLUDE_DIR})
target_link_directories(test_stereo_pipeline PRIVATE ${ONNXRUNTIME_LIB_DIR})
target_link_libraries(test_stereo_pipeline PRIVATE onnxruntime)
endif()

add_nexvr_gtest(test_temporal_pipeline tests/test_temporal_pipeline.cpp)
if(TARGET test_temporal_pipeline)
    target_include_directories(test_temporal_pipeline PRIVATE ${ONNXRUNTIME_INCLUDE_DIR})
    target_link_directories(test_temporal_pipeline PRIVATE ${ONNXRUNTIME_LIB_DIR})
    target_link_libraries(test_temporal_pipeline PRIVATE onnxruntime)
endif()

add_nexvr_gtest(test_vulkan_barrier_optimizer tests/test_vulkan_barrier_optimizer.cpp)
add_nexvr_gtest(test_vulkan_memory_budget tests/test_vulkan_memory_budget.cpp)
add_nexvr_gtest(test_vulkan_multiple_queues tests/test_vulkan_multiple_queues.cpp)
add_nexvr_gtest(test_frame_graph_profiler tests/test_frame_graph_profiler.cpp)
add_nexvr_gtest(test_vulkan_performance_snapshot tests/test_vulkan_performance_snapshot.cpp)
add_nexvr_gtest(test_vulkan_timeline_semaphore tests/test_vulkan_timeline_semaphore.cpp)
add_executable(dummy_json src/test_apps/dummy_json.cpp src/core/sprint_compatibility_logger.cpp)

target_include_directories(dummy_json PRIVATE ${JSON_INCLUDE_DIR})

add_executable(stress_injector tests/stress_injector.cpp)
target_include_directories(stress_injector PRIVATE ${PROJECT_SOURCE_DIR}/src)
if(MSVC)
    target_compile_options(stress_injector PRIVATE /EHa)
    target_compile_options(NexVRCore PRIVATE /EHa)
endif()
target_link_libraries(stress_injector PRIVATE NexVRCore)

# ---------------------------------------------------------------------------
# Auto-register standard GoogleTest suites
# ---------------------------------------------------------------------------
# Historically ~65 tests/test_*.cpp files existed on disk but were never wired
# into the build, so they silently never ran. This block registers every
# standard gtest suite automatically: any tests/test_*.cpp not already
# registered explicitly above (TARGET guard) and not a standalone main()
# harness (exclude list). CONFIGURE_DEPENDS re-runs the glob on build, so a new
# test file is picked up without a manual reconfigure. Suites needing ONNX
# Runtime or a custom main() stay registered explicitly above and are skipped
# here by the TARGET guard.
file(GLOB AUTO_GTEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/tests/test_*.cpp")

# Standalone harnesses that provide their own main() — linking gtest_main would
# duplicate the main symbol. Excluded from gtest auto-registration.
set(AUTO_GTEST_EXCLUDES
    test_openxr_dx12_swapchain
    test_stereo_visual
)

# Suites that do not compile against the current tree are quarantined here with
# a reason so they are VISIBLY skipped (logged at configure time) rather than
# silently breaking the build. Populated during the 67->100 test-registration
# triage; the goal is to drive this list back to empty.
set(AUTO_GTEST_QUARANTINE
    # --- Stale: reference types/members that no longer exist on the current API ---
    test_ai_pipeline                 # 'AICommandQueue' type no longer exists
    test_camera_tracker              # CameraDeltaTracker::UpdateDelta -> UpdateCandidateMotion (new signature)
    test_depth_tracker               # GraphicsResourceIdentity::creationGeneration field removed
    test_dx11_lifecycle              # 'Dx11ContextSnapshot' type removed
    # --- Toolchain: gmock 1.14 cannot mock __stdcall COM interfaces (static_assert) ---
    test_dx12_descriptor_tracker
    test_dx12_fence_manager
    test_dx12_resource_state_tracker
)

foreach(test_src IN LISTS AUTO_GTEST_SOURCES)
    get_filename_component(test_name ${test_src} NAME_WE)
    if(TARGET ${test_name})
        continue()
    endif()
    if(test_name IN_LIST AUTO_GTEST_EXCLUDES)
        continue()
    endif()
    if(test_name IN_LIST AUTO_GTEST_QUARANTINE)
        message(STATUS "Quarantined test (skipped, does not compile yet): ${test_name}")
        continue()
    endif()
    add_nexvr_gtest(${test_name} ${test_src})
endforeach()


