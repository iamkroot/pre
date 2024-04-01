cmake_minimum_required(VERSION 3.24)
project(prepp)

set(CMAKE_CXX_STANDARD 20)

find_package(LLVM 8.0.1 EXACT REQUIRED CONFIG PATHS ../llvm-project/build)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
