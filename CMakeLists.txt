cmake_minimum_required(VERSION 3.15)
project(tester LANGUAGES CXX)

# Create header-only library
add_library(tester INTERFACE)

# Expose include directory
target_include_directories(tester INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# Optional: require C++ standard
target_compile_features(tester INTERFACE cxx_std_20)
