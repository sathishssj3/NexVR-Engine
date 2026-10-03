# Global settings
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Security & Hardening compile/link options
if(MSVC)
    add_compile_options(/GS /guard:cf /EHa /MP)
    add_link_options(/DYNAMICBASE /NXCOMPAT /GUARD:CF /HIGHENTROPYVA)
else()
    add_compile_options(-fstack-protector-strong -D_FORTIFY_SOURCE=2)
    add_link_options(-Wl,-z,now -Wl,-z,relro)
endif()

if(NOT TARGET nexvr_warnings)
    add_library(nexvr_warnings INTERFACE)
    if(MSVC)
        target_compile_options(nexvr_warnings INTERFACE /W4)
    else()
        target_compile_options(nexvr_warnings INTERFACE -Wall -Wextra)
    endif()
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
