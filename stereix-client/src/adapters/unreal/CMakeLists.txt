# The Unreal Engine native RHI plugin bridge.
#
# A SEPARATE DLL from nexvr_sdk, not extra exports bolted onto it.
# The bridge links nexvr_sdk and provides the lock-free render command
# dispatching for Unreal Engine's ENQUEUE_RENDER_COMMAND.
add_library(nexvr_unreal SHARED
    nexvr_unreal_bridge.cpp
)

target_link_libraries(nexvr_unreal
    PRIVATE nexvr_warnings
            nexvr_sdk
            d3d11
            d3d12
)

target_compile_definitions(nexvr_unreal PRIVATE NEXVR_UNREAL_BUILD_DLL)
target_include_directories(nexvr_unreal PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/include")

set_target_properties(nexvr_unreal PROPERTIES FOLDER "SDK/Adapters")

# Generate stereix_unreal.dll & stereix_unreal.lib for seamless B2B partner linking
add_custom_command(TARGET nexvr_unreal POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:nexvr_unreal>"
        "$<TARGET_FILE_DIR:nexvr_unreal>/stereix_unreal.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_LINKER_FILE:nexvr_unreal>"
        "$<TARGET_LINKER_FILE_DIR:nexvr_unreal>/stereix_unreal.lib"
    COMMENT "Generating stereix_unreal.dll and stereix_unreal.lib alias binaries"
)
