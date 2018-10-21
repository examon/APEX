add_library(APEXPass MODULE apex.cpp apex.h)

OPTION(LLVM_DG "Support for LLVM Dependency graph" ON)
OPTION(ENABLE_CFG "Add support for CFG edges to the graph" ON)

target_compile_features(APEXPass PRIVATE cxx_range_for cxx_auto_type)
set_target_properties(APEXPass PROPERTIES COMPILE_FLAGS "-fno-rtti")

# This needs to be defined for dg
add_definitions(-DENABLE_CFG)
add_definitions(-DHAVE_LLVM)

# Getting dg_libs from top level CMakeLists.txt
target_link_libraries(APEXPass ${dg_libs})

target_link_libraries(APEXPass
        PRIVATE ${llvm_support}
        PRIVATE ${llvm_analysis}
        PRIVATE ${llvm_irreader}
        PRIVATE ${llvm_bitwriter}
        PRIVATE ${llvm_core})