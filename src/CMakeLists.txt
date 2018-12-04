cmake_minimum_required(VERSION 3.7)


# LLVM includes
find_package(LLVM REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})


# Include all dg headers and libraries

if(DG_INCLUDE_PATH)
else()
	set(DG_INCLUDE_PATH ${CMAKE_SOURCE_DIR}/dg/src)
endif()

if(DG_LIB_PATH)
else()
	set(DG_LIB_PATH ${CMAKE_SOURCE_DIR}/dg/build/src)
endif()

# DG_PATH is a path to an in-source build of dg
if (DG_PATH)
	message(STATUS "Setting-up paths to an in-source build of dg")
	set(DG_INCLUDE_PATH ${DG_PATH}/src)
	set(DG_LIB_PATH ${DG_PATH}/src)
endif()

include_directories(${DG_INCLUDE_PATH})
link_directories(${DG_LIB_PATH})

find_library(LIBPTA PTA PATH ${DG_LIB_PATH})
find_library(LIBRD RD PATH ${DG_LIB_PATH})
find_library(LIBLLVMPTA LLVMpta PATH ${DG_LIB_PATH})
find_library(LIBLLVMRD LLVMrd PATH ${DG_LIB_PATH})
find_library(LIBLLVMDG LLVMdg PATH ${DG_LIB_PATH})

set(dg_libs ${LIBPTA} ${LIBRD} ${LIBLLVMPTA} ${LIBLLVMRD} ${LIBLLVMDG})

# APEX

add_subdirectory(apex)
