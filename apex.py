#!/usr/bin/env python3

import argparse
import shlex
import subprocess
import sys

def execute(cmd, print_output=True):
    try:
        out = subprocess.check_output(cmd, shell=True)
        print(out.decode("utf-8"))
    except subprocess.CalledProcessError as e:
        sys.stderr.write(str(e) + '\n')
        return None

parser = argparse.ArgumentParser()

parser.add_argument('--code', type=str, help='input C source code compiled into LLVM bytecode')
parser.add_argument('--file', type=str, help='file where is the target (NOT FULL PATH)')
parser.add_argument('--line', type=int, help='target line number in file')


args = parser.parse_args()
code = args.code
target_file = args.file
line = args.line

print("code", code)
print("target_file", target_file)
print("line", line)


execute("rm -rf build; mkdir build")
execute("clang -O0 -g -c -emit-llvm src/apex/apexlib.c -o build/bytecode_from_apexlib.bc")
execute("llvm-link build/bytecode_from_apexlib.bc {INPUT} -S -o=build/linked_ir.ll".format(INPUT=code))
execute("llvm-as build/linked_ir.ll -o build/bytecode_from_linked_input.bc")


opt = """opt -o build/bytecode_from_apex.bc -load src/build/apex/libAPEXPass.so -apex -file={FILE} -line={LINE} < build/bytecode_from_linked_input.bc 2> build/apex.out
      """.format(FILE=target_file, LINE=line)
print(opt)
execute(opt)

