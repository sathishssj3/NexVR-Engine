# Stereix Engine Minimal C++ Direct3D 11 Integration Sample
cmake_minimum_required(VERSION 3.25)
project(StereixCppSample LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(stereix_sample_d3d11 minimal_stereix_d3d11.cpp)

target_include_directories(stereix_sample_d3d11 PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(stereix_sample_d3d11 PRIVATE
    nexvr_sdk
    d3d11
    dxgi
)

set_target_properties(stereix_sample_d3d11 PROPERTIES
    FOLDER "Samples"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/bin"
)
