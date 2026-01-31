cmake_minimum_required(VERSION 3.20)
project(ring_queue LANGUAGES CXX)

option(MY_LIB_BUILD_TESTS "Build tests" OFF)
option(CMAKE_EXPORT_COMPILE_COMMANDS OFF)

add_library(lockfree_ring_queue INTERFACE)
add_library(lockfree_ring_queue::lockfree_ring_queue ALIAS lockfree_ring_queue)

target_compile_features(lockfree_ring_queue INTERFACE cxx_std_20)

target_include_directories(lockfree_ring_queue INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
)

if(MY_LIB_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
