# The B2B SDK — a product, not an experiment.
#
# ===========================================================================
# THIS TARGET MUST NOT CONTAIN THE INJECTOR
# ===========================================================================
#
# NexVR's B2C path writes into other processes for a living: MinHook detours,
# CreateRemoteThread, memory scanners, proxy DLLs. A studio linking this SDK is
# shipping our binary inside their game, through their security review, to their
# players. None of that machinery may travel with it — not because it would
# malfunction, but because a licensed engine plugin containing process-injection
# primitives is a product nobody can ship.
#
# So this links openxr_loader, d3d11, and dxgi. It does NOT link nexvr_core,
# which would drag in minhook and the whole hooking surface transitively.
#
# The guarantee is enforced at the binary level by tests/test_sdk_imports.cpp,
# which reads the built DLL's PE import table — the same technique that keeps
# vrinject.dll free of networking. Intent is not a mechanism; the linker is.

add_library(nexvr_sdk SHARED
    nexvr_sdk.cpp      # the C ABI, deliberately thin
    sdk_session.cpp    # OpenXR + D3D11 core, promoted from the Phase 0.9 spike
    sdk_validation.cpp # the checks that turn silent failures into error codes
)

# PUBLIC: a consumer compiles against nexvr_sdk.h and nothing else in this tree.
target_include_directories(nexvr_sdk
    PUBLIC  "${CMAKE_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Switches NEXVR_API from dllimport to dllexport. Only ever set here.
target_compile_definitions(nexvr_sdk PRIVATE NEXVR_SDK_IMPLEMENTATION)

target_link_libraries(nexvr_sdk
    PRIVATE nexvr_warnings
            # Statically linked (DYNAMIC_LOADER OFF), so a studio ships
            # nexvr_sdk.dll alone rather than nexvr_sdk.dll plus a loader whose
            # resolution order depends on their game's directory layout.
            openxr_loader
            d3d11
            d3d12
            dxgi
)

set_target_properties(nexvr_sdk PROPERTIES FOLDER "SDK")
