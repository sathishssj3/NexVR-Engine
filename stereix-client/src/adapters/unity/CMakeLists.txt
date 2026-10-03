# The Stereix Unity native plugin bridge.
#
# A SEPARATE DLL from stereix_sdk, not extra exports bolted onto it. Two reasons:
#
#   - stereix_sdk.dll is engine-agnostic. A studio using Unreal, or their own
#     engine, should not be shipping Unity-shaped entry points they never call.
#   - test_sdk_imports asserts what stereix_sdk.dll may contain. Growing that DLL
#     an engine adapter at a time is how a clean import table stops being clean.
#
# The bridge links stereix_sdk and adds nothing of its own beyond the render-event
# plumbing, so it inherits the same no-injection guarantee.
add_library(stereix_unity SHARED
    nexvr_unity_bridge.cpp
)

# Backwards-compatibility alias
add_library(nexvr_unity ALIAS stereix_unity)

target_link_libraries(stereix_unity
    PRIVATE nexvr_warnings
            stereix_sdk
            d3d11
            d3d12
)

target_compile_definitions(stereix_unity PRIVATE NEXVR_UNITY_BUILD_DLL STEREIX_UNITY_BUILD_DLL)
target_include_directories(stereix_unity PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/include")

set_target_properties(stereix_unity PROPERTIES FOLDER "SDK/Adapters")

# Backward compatibility: copy stereix_unity to nexvr_unity alias
add_custom_command(TARGET stereix_unity POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:stereix_unity>"
        "$<TARGET_FILE_DIR:stereix_unity>/nexvr_unity.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_LINKER_FILE:stereix_unity>"
        "$<TARGET_LINKER_FILE_DIR:stereix_unity>/nexvr_unity.lib"
    COMMENT "Generating nexvr_unity.dll and nexvr_unity.lib legacy alias binaries"
)
