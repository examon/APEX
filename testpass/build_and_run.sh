rm -rf build
mkdir build
cd build
cmake ..
make
cd ..

#clang-format -style=google -i testpass/*.cpp

#clang -Xclang -load -Xclang build/skeleton/libSkeletonPass.* c_code/test.c
clang -O0 -c -emit-llvm c-code/test.c -o build/test.bc

opt -o build/test_after_opt.bc -mem2reg -dce -loop-simplify -simplifycfg -load build/testpass/libTestPass.so -testpass < build/test.bc > /dev/null
llvm-dis build/test_after_opt.bc
#lli build/test_after_opt.bc

