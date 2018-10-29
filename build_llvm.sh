#!/bin/bash
# Needs to run as a root

LLVM_VERSION=7.0.0

rm -rf llvm
mkdir llvm
cd llvm

curl -SL http://llvm.org/releases/${LLVM_VERSION}/cfe-${LLVM_VERSION}.src.tar.xz | tar xJ
#curl -SL https://github.com/viktormalik/llvm/archive/diffkemp.tar.gz | tar xz
#curl -SL https://github.com/examon/llvm/archive/diffkemp.tar.gz | tar xz
#git clone https://github.com/examon/llvm
curl -SL https://releases.llvm.org/${LLVM_VERSION}/llvm-${LLVM_VERSION}.src.tar.xz | tar xJ

mv llvm-${LLVM_VERSION}.src llvm
mv cfe-${LLVM_VERSION}.src llvm/tools/clang
cd llvm
mkdir build
cd build

cmake .. -GNinja \
         -DCMAKE_BUILD_TYPE=Release \
         -DLLVM_ENABLE_ASSERTIONS=ON \
         -DCMAKE_INSTALL_PREFIX=/usr/local \
         -DLLVM_PARALLEL_LINK_JOBS=1 \
         -DLLVM_TARGETS_TO_BUILD=X86 \
         -DLLVM_BUILD_LLVM_DYLIB=ON
ninja -j9 && ninja install
