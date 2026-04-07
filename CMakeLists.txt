cmake_minimum_required(VERSION 3.15)
project(emi-editor
  LANGUAGES CXX
  VERSION 1.0
)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_subdirectory(dependencies)
add_subdirectory(src)

option(BUILD_TESTS "Build tests" OFF)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()