INPUT_SOURCE=example.c
BYTECODE_FROM_INPUT=example.bc

build:
	rm -rf *.bc
	clang -O0 -g -c -emit-llvm ${INPUT_SOURCE} -o ${BYTECODE_FROM_INPUT}
