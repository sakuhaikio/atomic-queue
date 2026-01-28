cmake_minimum_required(VERSION 3.15)
project(atomic_queue LANGUAGES CXX)

add_library(atomic_queue INTERFACE)

target_compile_features(atomic_queue INTERFACE cxx_std_20)

target_include_directories(atomic_queue INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
)

