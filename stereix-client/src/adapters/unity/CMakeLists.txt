# The Unity native plugin bridge.
#
# A SEPARATE DLL from nexvr_sdk, not extra exports bolted onto it. Two reasons:
#
#   - nexvr_sdk.dll is engine-agnostic. A studio using Unreal, or their own
#     engine, should not be shipping Unity-shaped entry points they never call.
#   - test_sdk_imports asserts what nexvr_sdk.dll may contain. Growing that DLL
#     an engine adapter at a time is how a clean import table stops being clean.
#
# The bridge links nexvr_sdk and adds nothing of its own beyond the render-event
# plumbing, so it inherits the same no-injection guarantee.
add_library(nexvr_unity SHARED
    nexvr_unity_bridge.cpp
)

target_link_libraries(nexvr_unity
    PRIVATE nexvr_warnings
            nexvr_sdk
            d3d11
            d3d12
)

target_compile_definitions(nexvr_unity PRIVATE NEXVR_UNITY_BUILD_DLL)
target_include_directories(nexvr_unity PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/include")

set_target_properties(nexvr_unity PROPERTIES FOLDER "SDK/Adapters")
