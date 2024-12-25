cmake_minimum_required(VERSION 3.14)

project(opengltest)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 引入 FetchContent 模块
include(FetchContent)

# 定义 fmt 库的依赖
FetchContent_Declare(
    fmt
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/fmt
)

# 拉取并集成 fmt
FetchContent_MakeAvailable(fmt)

find_package(OpenMP)
if(OPENMP_FOUND)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

#set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-checks=-*,modernize-*")
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/W4)
endif()

add_executable(main main.cpp)
target_include_directories(main PRIVATE "3rdparty")
target_link_libraries(main PRIVATE X11 fmt)

add_executable(test test.cpp)
target_include_directories(test PRIVATE "3rdparty")
target_link_libraries(test PRIVATE X11 fmt)
