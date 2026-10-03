cmake_minimum_required(VERSION 3.25)
project(StereixEngine LANGUAGES C CXX)

# -----------------------------------------------------------------------------
# Stereix Engine Monorepo Root CMake Orchestrator
# Directs compilation to stereix-client while preserving root build layout
# -----------------------------------------------------------------------------
enable_testing()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/stereix-client")
    add_subdirectory(stereix-client)
else()
    add_subdirectory(nexvr-client)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/samples/cpp")
    add_subdirectory(samples/cpp)
endif()
