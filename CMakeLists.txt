cmake_minimum_required(VERSION 3.16)
project(raftcore CXX)

find_package(Threads REQUIRED)

include(${CMAKE_SOURCE_DIR}/cmake/clang-tools.cmake)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -g")

set(CMAKE_CXX_STANDARD 17) # 使用 C++17 标准
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # 生成 compile_commands.json，防止无法识别 include
set(CMAKE_BUILD_TYPE Debug) # 指定编译选项

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin) # 指定可执行文件输出目录
set(PROJECT_BINARY_DIR ${CMAKE_SOURCE_DIR}/build) # 指定编译目录

set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)

include_directories(${CMAKE_SOURCE_DIR}/src) # 指定头文件目录
include_directories(${CMAKE_SOURCE_DIR}/third_party/include)

link_directories(${CMAKE_SOURCE_DIR}/third_party/lib)

add_subdirectory(${CMAKE_SOURCE_DIR}/src)
add_subdirectory(${CMAKE_SOURCE_DIR}/test)

set(gtest_force_shared_crt
    ON
    CACHE BOOL "" FORCE) # don't override our compiler/linker options when
                         # building gtest