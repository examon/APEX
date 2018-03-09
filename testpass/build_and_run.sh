rm -rf build
mkdir build
cd build
cmake ..
make
cd ..

clang-format -style=google -i testpass/*.cpp


# compile without anything
clang -O0 -c -emit-llvm c-code/test.c -o build/test.bc


# run some basic optmizations, so the IR is cleaner
opt -o build/test_after_default_opt.bc -mem2reg -dce -loop-simplify -simplifycfg build/test.bc

# run pass
opt -o build/test_after_testpass.bc -mem2reg -dce -loop-simplify -simplifycfg -load build/testpass/libTestPass.so -testpass < build/test_after_default_opt.bc > /dev/null

# disassemble bytecode so I can look inside
llvm-dis build/test.bc
llvm-dis build/test_after_default_opt.bc
llvm-dis build/test_after_testpass.bc


echo "<<<<<< START: test_after_testpass.ll contents >>>>>>"
cat build/test_after_testpass.ll
echo "<<<<<< END: test_after_testpass.ll contents >>>>>>"

# run pass and execute modified bytecode
echo "<<<<<< running test_after_testpass.bc >>>>>>"
lli build/test_after_testpass.bc

