cmake_minimum_required(VERSION 3.25)
project(StereixEngine LANGUAGES C CXX)

# -----------------------------------------------------------------------------
# Stereix Engine Monorepo Root CMake Orchestrator
# Directs compilation to nexvr-client while preserving root build layout
# -----------------------------------------------------------------------------
enable_testing()
add_subdirectory(nexvr-client)
