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

# run our pass
opt -o build/test_after_testpass.bc -load build/testpass/libTestPass.so -testpass < build/test_after_default_opt.bc > /dev/null

# run callgraph exporter
cd build

opt -dot-callgraph test_after_default_opt.bc
mv callgraph.dot callgraph_default_opt.dot
dot callgraph_default_opt.dot -Tsvg -Tsvgz -O

opt -dot-callgraph test_after_testpass.bc
mv callgraph.dot callgraph_testpass.dot
dot callgraph_testpass.dot -Tsvg -Tsvgz -O

cd ..
# callgraph export end




# disassemble bytecode so I can look inside
llvm-dis build/test.bc
llvm-dis build/test_after_default_opt.bc
llvm-dis build/test_after_testpass.bc

# info output
#echo "<<<<<< START: test_after_testpass.ll contents >>>>>>"
#cat build/test_after_testpass.ll
#echo "<<<<<< END: test_after_testpass.ll contents >>>>>>"

# run pass and execute modified bytecode
echo "<<<<<< running test_after_testpass.bc >>>>>>"
lli build/test_after_testpass.bc

