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
