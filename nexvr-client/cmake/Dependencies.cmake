# Dependencies – MinHook (inline hooking library)
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG v1.3.3
)
FetchContent_MakeAvailable(minhook)
set(MINHOOK_INCLUDE_DIR ${minhook_SOURCE_DIR}/include)
set(MINHOOK_SOURCES
    ${minhook_SOURCE_DIR}/src/buffer.c
    ${minhook_SOURCE_DIR}/src/hook.c
    ${minhook_SOURCE_DIR}/src/trampoline.c
    ${minhook_SOURCE_DIR}/src/hde/hde32.c
    ${minhook_SOURCE_DIR}/src/hde/hde64.c
)


# ---------------------------------------------------------------------------
# Dependencies – ONNX Runtime (DirectML)
# ---------------------------------------------------------------------------
# We download the pre-compiled ONNX Runtime DirectML release for Windows x64 via NuGet.
FetchContent_Declare(
    onnxruntime
    URL https://www.nuget.org/api/v2/package/Microsoft.ML.OnnxRuntime.DirectML/1.16.3
)
FetchContent_MakeAvailable(onnxruntime)
set(ONNXRUNTIME_INCLUDE_DIR ${onnxruntime_SOURCE_DIR}/build/native/include)
set(ONNXRUNTIME_LIB_DIR ${onnxruntime_SOURCE_DIR}/runtimes/win-x64/native)

# We also need the DirectML redist DLL
FetchContent_Declare(
    directml
    URL https://www.nuget.org/api/v2/package/Microsoft.AI.DirectML/1.13.1
)
FetchContent_MakeAvailable(directml)

# ---------------------------------------------------------------------------
# Dependencies – Vulkan SDK
# ---------------------------------------------------------------------------
find_package(Vulkan REQUIRED)

# ---------------------------------------------------------------------------
# Dependencies – OpenXR SDK
# ---------------------------------------------------------------------------
FetchContent_Declare(
    openxr
    GIT_REPOSITORY https://github.com/KhronosGroup/OpenXR-SDK.git
    GIT_TAG release-1.0.34
    PATCH_COMMAND powershell -Command "(Get-Content CMakeLists.txt) -replace 'find_package\\(PythonInterp 3\\)', 'find_package(Python3 COMPONENTS Interpreter)`nset(PYTHON_EXECUTABLE `$`{Python3_EXECUTABLE})' | Set-Content CMakeLists.txt"
)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_CONFORMANCE OFF CACHE BOOL "" FORCE)
set(BUILD_LOADER ON CACHE BOOL "" FORCE)
set(BUILD_WITH_SHARED_CRT ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(openxr)
set(OPENXR_INCLUDE_DIR ${openxr_SOURCE_DIR}/include)

# ---------------------------------------------------------------------------
# Dependencies – nlohmann/json
# ---------------------------------------------------------------------------
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)
set(JSON_INCLUDE_DIR ${nlohmann_json_SOURCE_DIR}/include)

# ---------------------------------------------------------------------------
# Dependencies - GoogleTest
# ---------------------------------------------------------------------------
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ---------------------------------------------------------------------------
# Dependencies – Dear ImGui
# ---------------------------------------------------------------------------
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.5
)
FetchContent_MakeAvailable(imgui)
set(IMGUI_INCLUDE_DIR ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)


# ---------------------------------------------------------------------------
