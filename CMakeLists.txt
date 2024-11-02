cmake_minimum_required(VERSION 3.2)

project(project LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD_REQUIRED on)


add_executable(project
    main.cpp
)
