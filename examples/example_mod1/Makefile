INPUT_SOURCE=example_mod1.c
BYTECODE_FROM_INPUT=example_mod1.bc

build:
	rm -rf *.bc
	clang -O0 -g -c -emit-llvm ${INPUT_SOURCE} -o ${BYTECODE_FROM_INPUT}
