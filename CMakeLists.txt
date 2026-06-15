cmake_minimum_required(VERSION 3.25)
project(VRInject VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Global settings
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
    ${minhook_SOURCE_DIR}/src/hde/hde32.c
    ${minhook_SOURCE_DIR}/src/hde/hde64.c
)

# ---------------------------------------------------------------------------
# Dependencies – ONNX Runtime (DirectML)
# ---------------------------------------------------------------------------
# We download the pre-compiled ONNX Runtime DirectML release for Windows x64.
# (URL is a placeholder for the actual GitHub release zip)
FetchContent_Declare(
    onnxruntime
    URL https://github.com/microsoft/onnxruntime/releases/download/v1.16.3/onnxruntime-win-x64-1.16.3.zip
)
FetchContent_GetProperties(onnxruntime)
if(NOT onnxruntime_POPULATED)
    FetchContent_Populate(onnxruntime)
endif()
set(ONNXRUNTIME_INCLUDE_DIR ${onnxruntime_SOURCE_DIR}/include)
set(ONNXRUNTIME_LIB_DIR ${onnxruntime_SOURCE_DIR}/lib)

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

find_program(DXC_EXECUTABLE dxc)
if(NOT DXC_EXECUTABLE)
    message(WARNING "dxc.exe not found. DX12 shaders will not be recompiled.")
endif()

set(SHADER_SOURCE_DIR ${CMAKE_SOURCE_DIR}/shaders)
set(SHADER_OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin/shaders)

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
)

# Create output directory for compiled shaders.
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

# Copy the raw .hlsl files to the output directory so the launcher can deploy them for dynamic compilation
foreach(shader_src ${SHADER_SOURCES})
    get_filename_component(shader_name ${shader_src} NAME)
    configure_file(${shader_src} ${SHADER_OUTPUT_DIR}/${shader_name} COPYONLY)
endforeach()

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

            # set(SHADER_HDR_VK ${SHADER_OUTPUT_DIR}/${SHADER_NAME}_cs_vk.h)
            # list(APPEND SHADER_OUTPUTS ${SHADER_HDR_VK})

            # Vulkan SPIR-V shader compilation disabled because default Windows Kits dxc doesn't support it
            # and we only need D3D11 for Stumble Guys.
            # add_custom_command(
            #     OUTPUT  ${SHADER_HDR_VK}
            #     COMMAND ${DXC_EXECUTABLE}
            #             -spirv
            #             -T cs_6_0
            #             -E CSMain
            #             -O3
            #             -fspv-target-env=vulkan1.1
            #             -Fh ${SHADER_HDR_VK}
            #             -Vn g_${SHADER_NAME}_VK
            #             ${SHADER_SRC}
            #     DEPENDS ${SHADER_SRC}
            #     COMMENT "Compiling HLSL compute shader to SPIR-V: ${SHADER_NAME}.hlsl -> ${SHADER_NAME}_cs_vk.h"
            #     VERBATIM
            # )
        endif()
    endforeach()
endif()

# A convenience target so shaders rebuild when sources change.
add_custom_target(compile_shaders ALL DEPENDS ${SHADER_OUTPUTS})

# ---------------------------------------------------------------------------
# vrinject – the injected DLL
# ---------------------------------------------------------------------------
add_library(vrinject SHARED
    src/dllmain.cpp
    src/core/logger.h
    src/core/engine_detector.h
    src/core/engine_detector.cpp
    src/core/drm_manager.h
    src/core/drm_manager.cpp
    src/core/hook_manager.cpp
    src/hooks/unreal_hook.cpp
    src/hooks/unity_hook.cpp
    src/core/memory_scanner.h
    src/core/memory_scanner.cpp
    src/core/ue_sdk.h
    src/rendering/irenderer.h
    src/rendering/backends/dx11_renderer.h
    src/rendering/backends/dx11_renderer.cpp
    src/rendering/backends/dx12_renderer.h
    src/rendering/backends/dx12_renderer.cpp
    src/rendering/backends/vulkan_renderer.h
    src/rendering/backends/vulkan_renderer.cpp
    src/hooks/dx11_hook.cpp
    src/hooks/dx12_hook.h
    src/hooks/dx12_hook.cpp
    src/hooks/dxgi_factory_hook.h
    src/hooks/dxgi_factory_hook.cpp
    src/hooks/vulkan_layer.cpp
    src/rendering/stereo_pipeline.h
    src/rendering/stereo_pipeline.cpp
    src/rendering/comfort_guard.h
    src/rendering/comfort_guard.cpp
    src/rendering/depth_reprojector.cpp
    src/rendering/neural_inpainter.h
    src/rendering/neural_inpainter.cpp
    src/rendering/openxr_manager.h
    src/rendering/openxr_manager.cpp
    src/rendering/cross_adapter_copier.h
    src/rendering/cross_adapter_copier.cpp
    src/core/motion_predictor.h
    src/core/motion_predictor.cpp
    src/hooks/input_hook.h
    src/hooks/input_hook.cpp
    src/hooks/input_manager.h
    src/hooks/input_manager.cpp
    src/hooks/audio_hook.cpp
    src/core/matrix_classifier.h
    src/core/matrix_classifier.cpp
    src/core/config_manager.h
    src/core/config_manager.cpp
    ${MINHOOK_SOURCES}
    ${IMGUI_SOURCES}
)

# ---------------------------------------------------------------------------
# Proxy DLL (dxgi.dll)
# ---------------------------------------------------------------------------
add_library(dxgi_proxy SHARED
    src/proxy/dxgi_proxy.cpp
)
set_target_properties(dxgi_proxy PROPERTIES OUTPUT_NAME "dxgi")
target_link_libraries(dxgi_proxy PRIVATE user32)

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
)

# Post-build step to copy onnxruntime.dll to output bin dir
add_custom_command(TARGET vrinject POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${onnxruntime_SOURCE_DIR}/lib/onnxruntime.dll"
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
# injector – standalone process injector tool
# ---------------------------------------------------------------------------
add_executable(injector tools/injector.cpp)
set_target_properties(injector PROPERTIES LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\"")
target_link_libraries(injector PRIVATE
    # No extra libs needed – the Win32 APIs we use are in kernel32 which is
    # linked implicitly.
)

# S1.1: Code-sign injector.exe
if (DEFINED ENV{SIGN_CERT_PATH})
    add_custom_command(TARGET injector POST_BUILD
      COMMAND signtool.exe sign
        /f "$ENV{SIGN_CERT_PATH}"
        /p "$ENV{SIGN_CERT_PASS}"
        /tr http://timestamp.digicert.com
        /td sha256 /fd sha256
        "$<TARGET_FILE:injector>"
      COMMENT "Signing injector.exe")
endif()

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

target_link_libraries(dummy_app PRIVATE
    d3d11
    user32
)

# ---------------------------------------------------------------------------
# test_app - testing vrinject DLL dynamically
# ---------------------------------------------------------------------------
add_executable(test_app WIN32 test_app/test_app.cpp)
target_link_libraries(test_app PRIVATE d3d11 dxgi user32)

add_executable(test_xr src/test_xr.cpp)
target_include_directories(test_xr PRIVATE ${OPENXR_INCLUDE_DIR})
target_link_libraries(test_xr PRIVATE openxr_loader)
