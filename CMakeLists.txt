cmake_minimum_required(VERSION 3.25)
project(NexVREngine LANGUAGES C CXX)

# -----------------------------------------------------------------------------
# NexVR Monorepo Root CMake Orchestrator
# Directs compilation to nexvr-client while preserving root build layout
# -----------------------------------------------------------------------------
add_subdirectory(nexvr-client)
