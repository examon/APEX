#!/usr/bin/env python3

# Created by Tomas Meszaros (exo at tty dot com, tmeszaro at redhat dot com)
#
# Published under Apache 2.0 license.
# See LICENSE for details.

"""
python3 apex.py examples/mod1/example_mod1.bc example_mod1.c 11 --export=true

output:

    extracted: binary program extracted from the example_mod1.bc
    build/*: directory full of interesting files, logs, etc.
"""

import argparse
import os
import subprocess
import sys

def execute(cmd, print_output=False):
    try:
        out = subprocess.check_output(cmd, shell=True)
        if print_output:
            print(out.decode("utf-8"))
    except subprocess.CalledProcessError as e:
        sys.stderr.write(str(e) + '\n')
        return None

# Config cmd line args.
parser = argparse.ArgumentParser()
parser.add_argument("code", type=str, help="C source code compiled into LLVM bytecode.")
parser.add_argument("file", type=str, help="Target file name (NOT FULL PATH).")
parser.add_argument("line", type=int, help="Target line number.")
parser.add_argument("--export", type=str, help="true/false for exporting call graphs.")

# Parse cmd line args.
args = parser.parse_args()
code = args.code
target_file = args.file
line = args.line
export = args.export

execute("rm -rf build extracted; mkdir build")

# Compile apexlib to the bytecode.
execute("clang -O0 -g -c -emit-llvm src/apex/apexlib.c -o build/apexlib.bc")

# Link @code with apexlib bytecode.
execute("llvm-link build/apexlib.bc {INPUT} -S -o=build/linked.ll".format(INPUT=code))

# Procedure bytecode from linked input and apexlib.
execute("llvm-as build/linked.ll -o build/linked.bc")

# Run APEXPass on the linked bytecode we produced above.
# Save log to the build/apex.log.
if not os.path.isfile("src/build/apex/libAPEXPass.so"):
    print("ERROR: Please first build APEX with: make build")
    sys.exit(1)
opt = """opt -o build/apex.bc -load src/build/apex/libAPEXPass.so -apex -file={FILE} -line={LINE} < build/linked.bc 2> build/apex.log
      """.format(FILE=target_file, LINE=line)
execute(opt)

# Compile apex.bc into executable called "extracted"
execute("clang -o extracted build/apex.bc")

# Disassembly apexlib and final extracted bytecode for dbg & logging purposes.
execute("llvm-dis build/apexlib.bc -o build/apexlib.ll")
execute("llvm-dis build/apex.bc -o build/apex.ll")

# Optional call graphs export
if export and export == "true":
    execute("rm -rf build/callgraphs; mkdir build/callgraphs")
    execute("opt -dot-callgraph {INPUT} > /dev/null".format(INPUT=code))
    execute("mv callgraph.dot callgraph_no_opt.dot")
    execute("dot callgraph_no_opt.dot -Tsvg -O")
    execute("rm -rf callgraph_no_opt.dot")
    execute("mv callgraph_no_opt.dot.svg build/callgraphs")

    execute("opt -dot-callgraph build/linked.bc > /dev/null")
    execute("mv callgraph.dot callgraph_linked.dot")
    execute("dot callgraph_linked.dot -Tsvg -O")
    execute("rm -rf callgraph_linked.dot")
    execute("mv callgraph_linked.dot.svg build/callgraphs")

    execute("opt -dot-callgraph build/apex.bc > /dev/null")
    execute("mv callgraph.dot callgraph_apex.dot")
    execute("dot callgraph_apex.dot -Tsvg -O")
    execute("rm -rf callgraph_apex.dot")
    execute("mv callgraph_apex.dot.svg build/callgraphs")
