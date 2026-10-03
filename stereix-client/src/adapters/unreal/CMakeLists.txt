# The Stereix Unreal Engine native RHI plugin bridge.
#
# A SEPARATE DLL from stereix_sdk, not extra exports bolted onto it.
# The bridge links stereix_sdk and provides the lock-free render command
# dispatching for Unreal Engine's ENQUEUE_RENDER_COMMAND.
add_library(stereix_unreal SHARED
    nexvr_unreal_bridge.cpp
)

# Backwards-compatibility alias
add_library(nexvr_unreal ALIAS stereix_unreal)

target_link_libraries(stereix_unreal
    PRIVATE nexvr_warnings
            stereix_sdk
            d3d11
            d3d12
)

target_compile_definitions(stereix_unreal PRIVATE NEXVR_UNREAL_BUILD_DLL STEREIX_UNREAL_BUILD_DLL)
target_include_directories(stereix_unreal PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/include")

set_target_properties(stereix_unreal PROPERTIES FOLDER "SDK/Adapters")

# Backward compatibility: copy stereix_unreal to nexvr_unreal alias
add_custom_command(TARGET stereix_unreal POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:stereix_unreal>"
        "$<TARGET_FILE_DIR:stereix_unreal>/nexvr_unreal.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_LINKER_FILE:stereix_unreal>"
        "$<TARGET_LINKER_FILE_DIR:stereix_unreal>/nexvr_unreal.lib"
    COMMENT "Generating nexvr_unreal.dll and nexvr_unreal.lib legacy alias binaries"
)
