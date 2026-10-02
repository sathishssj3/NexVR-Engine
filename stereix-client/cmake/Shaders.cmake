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

set(SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
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
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/shaders/vulkan DESTINATION ${SHADER_OUTPUT_DIR})

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

# Workaround for dx12_renderer.cpp hardcoded path without touching src/rendering
add_custom_target(copy_shaders_for_dx12 ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${SHADER_OUTPUT_DIR}
        ${CMAKE_BINARY_DIR}/bin/shaders
    DEPENDS compile_shaders
)

