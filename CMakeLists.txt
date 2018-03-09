# cmake_minimum_required(VERSION <specify CMake version here>)
project(llvm_stuff)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        testpass/c-code/test.c
        testpass/testpass/testpass.cpp)

add_executable(llvm_stuff ${SOURCE_FILES})