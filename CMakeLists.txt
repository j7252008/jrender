cmake_minimum_required (VERSION 3.8)

project(opengltest)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#include_directories("")

#link_directories("")

add_executable(main main.cpp)
target_link_libraries(main X11)


add_executable(test test.cpp)