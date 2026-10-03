# The Stereix B2B SDK — a product, not an experiment.
#
# ===========================================================================
# THIS TARGET MUST NOT CONTAIN THE INJECTOR
# ===========================================================================
#
# A studio linking this SDK is shipping our binary inside their game, through
# their security review, to their players. None of that process-injection
# machinery may travel with it — not because it would malfunction, but because
# a licensed engine plugin containing process-injection primitives is a product
# nobody can ship.
#
# So this links openxr_loader, d3d11, d3d12, and dxgi. It does NOT link NexVRCore,
# which would drag in minhook and the whole hooking surface transitively.
#
# The guarantee is enforced at the binary level by tests/test_sdk_imports.cpp,
# which reads the built DLL's PE import table. Intent is not a mechanism;
# the linker is.

add_library(stereix_sdk SHARED
    nexvr_sdk.cpp      # the C ABI, deliberately thin
    sdk_session.cpp    # OpenXR + D3D11 core
    sdk_validation.cpp # the checks that turn silent failures into error codes
)

# Backwards-compatibility alias for internal targets
add_library(nexvr_sdk ALIAS stereix_sdk)

# PUBLIC: a consumer compiles against stereix_sdk.h and nothing else in this tree.
target_include_directories(stereix_sdk
    PUBLIC  "${CMAKE_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Switches NEXVR_API and STEREIX_API from dllimport to dllexport.
target_compile_definitions(stereix_sdk PRIVATE NEXVR_SDK_IMPLEMENTATION STEREIX_SDK_IMPLEMENTATION)

target_link_libraries(stereix_sdk
    PRIVATE nexvr_warnings
            openxr_loader
            d3d11
            d3d12
            dxgi
)

set_target_properties(stereix_sdk PROPERTIES FOLDER "SDK")

# Backward compatibility: copy stereix_sdk to nexvr_sdk alias
add_custom_command(TARGET stereix_sdk POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:stereix_sdk>"
        "$<TARGET_FILE_DIR:stereix_sdk>/nexvr_sdk.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_LINKER_FILE:stereix_sdk>"
        "$<TARGET_LINKER_FILE_DIR:stereix_sdk>/nexvr_sdk.lib"
    COMMENT "Generating nexvr_sdk.dll and nexvr_sdk.lib legacy alias binaries"
)
