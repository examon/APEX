# cmake_minimum_required(VERSION <specify CMake version here>)
project(apex)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        apex/c-code/test.c
        apex/apex/apex.cpp)

add_executable(llvm_stuff ${SOURCE_FILES})