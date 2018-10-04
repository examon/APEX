#!/bin/bash

# Created by Tomas Meszaros (exo at tty dot com, tmeszaro at redhat dot com)
#
# Published under Apache 2.0 license.
# See LICENSE for details.

# What is this:
# Script for building APEX as LLVM pass and then generating bytecode from
# input before and after running APEX (+running code formatting,
# generating images, etc.)


# TODO: This should be part of user input and not hardcoded.
INPUT_SOURCE="test_dependencies_minimal.c"
SOURCE_FCN="main"
TARGET_FCN="x"

# Generated bytecodes during all stages of processing.
BYTECODE_FROM_INPUT="bytecode_from_input.bc" # No optimizations.
BYTECODE_FROM_INPUT_BASIC_OPTS="bytecode_from_input_basic_opts.bc" # Some basic optimizations (see BASIC_OPTS).
BYTECODE_FROM_INPUT_BASIC_OPTS_AND_APEX="bytecode_from_input_basic_opts_and_apex.bc" # BASIC_OPTS + APEXPass.

# Optimizations that are ran before APEXPass
BASIC_OPTS="-dce -loop-simplify -simplifycfg"


# =============================================================================
# "So it begins..." ===========================================================

# Build dg
cd dg
rm -rf build
mkdir build
cd build
cmake ..
make -j9 # compiling really fast here dude
cd ../..

# Build APEX
rm -rf build
mkdir build
cd build
#cmake .. -LAH # verbose
cmake ..
make -j9
cd ..


# =============================================================================
# Running code formatter ======================================================

clang-format -style=llvm -i apex/*.cpp apex/*.h


# =============================================================================
# Compile to bytecode and run basic opts and than our opt =====================

# compile without anything
clang -O0 -c -emit-llvm c-code/${INPUT_SOURCE} -o build/${BYTECODE_FROM_INPUT}

# run some basic optimizations, so the IR is cleaner
opt -o build/${BYTECODE_FROM_INPUT_BASIC_OPTS} ${BASIC_OPTS} build/${BYTECODE_FROM_INPUT} > /dev/null

# run our pass
opt -o build/${BYTECODE_FROM_INPUT_BASIC_OPTS_AND_APEX} -load build/apex/libAPEXPass.so -apex -source=${SOURCE_FCN} -target=${TARGET_FCN} < build/${BYTECODE_FROM_INPUT_BASIC_OPTS}  > /dev/null


# ==============================================================================
# Generate callgraphs before and after =========================================

echo "========= exporting callgraphs"

# run callgraph exporter
cd build
rm -rf callgraphs
mkdir callgraphs
cd callgraphs

opt -dot-callgraph ../${BYTECODE_FROM_INPUT} > /dev/null
mv callgraph.dot callgraph_no_opt.dot
dot callgraph_no_opt.dot -Tsvg -O

opt -dot-callgraph ../${BYTECODE_FROM_INPUT_BASIC_OPTS} > /dev/null
mv callgraph.dot callgraph_default_opt.dot
dot callgraph_default_opt.dot -Tsvg -O

opt -dot-callgraph ../${BYTECODE_FROM_INPUT_BASIC_OPTS_AND_APEX} > /dev/null
mv callgraph.dot callgraph_apex_opt.dot
dot callgraph_apex_opt.dot -Tsvg -O

cd ../..


# ==============================================================================
# DG tools output ==============================================================

echo "========= running dg tools"

cd build
rm -rf dg_tools
mkdir dg_tools
cd dg_tools


#../../dg/build/tools/llvm-dg-dump ../${BYTECODE_FROM_INPUT} > llvm_dg_dump_no_opt.dot
#dot llvm_dg_dump_no_opt.dot -Tpdf -O

../../dg/build/tools/llvm-dg-dump -no-control ../${BYTECODE_FROM_INPUT} > llvm_dg_dump_no_control_no_opt.dot
dot llvm_dg_dump_no_control_no_opt.dot -Tpdf -O

#../../dg/build/tools/llvm-dg-dump -no-data ../${BYTECODE_FROM_INPUT} > llvm_dg_dump_no_data_no_opt.dot
#dot llvm_dg_dump_no_data_no_opt.dot -Tpdf -O

#../../dg/build/tools/llvm-dg-dump -bb-only ../${BYTECODE_FROM_INPUT} > llvm_dg_dump_bb_only_no_opt.dot
#dot llvm_dg_dump_bb_only_no_opt.dot -Tpdf -O

#../../dg/build/tools/llvm-ps-dump -dot ../${BYTECODE_FROM_INPUT} > llvm_ps_dump_no_opt.dot
#dot llvm_ps_dump_no_opt.dot -Tsvgz -O

#../../dg/build/tools/llvm-rd-dump -dot ../${BYTECODE_FROM_INPUT} > llvm_rd_dump_no_opt.dot
#dot llvm_rd_dump_no_opt.dot -Tpdf -O

cd ../..


# ==============================================================================
# Disassemble bytecode so We can look inside ===================================

llvm-dis build/${BYTECODE_FROM_INPUT}
llvm-dis build/${BYTECODE_FROM_INPUT_BASIC_OPTS}
llvm-dis build/${BYTECODE_FROM_INPUT_BASIC_OPTS_AND_APEX}


# ==============================================================================
# Run generated bytecodes ======================================================

# run bytecode after basic opts
echo "========= running ${BYTECODE_FROM_INPUT}"
lli build/${BYTECODE_FROM_INPUT}

# run bytecode after basic opts
echo "========= running ${BYTECODE_FROM_INPUT_BASIC_OPTS}"
lli build/${BYTECODE_FROM_INPUT_BASIC_OPTS}

# run pass and execute modified bytecode
echo "========= running ${BYTECODE_FROM_INPUT_BASIC_OPTS_AND_APEX}"
lli build/${BYTECODE_FROM_INPUT_BASIC_OPTS_AND_APEX}
