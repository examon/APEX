# APEX - Active code Path EXtractor

APEX is a tool that can extract active code path from the C program source code
and recompile this path into new, standalone executable.


### Getting started

In order to run APEX, you need to have llvm installed:
- https://llvm.org/docs/GettingStarted.html
- https://releases.llvm.org/3.8.0/docs/CMake.html

If you want to build LLVM from source, run:
``` shell
cd src
./build_llvm.sh
```

After getting LLVM, APEX needs to be built. Run the following command
to build APEX:
``` shell
make build
```

### How to run APEX

First, you need to compile your C program into LLVM bitcode since APEX cannot
work directly with the C source code.

``` bash
clang -c -g -emit-llvm main.c -o main.bc
```

After obtaining bitcode, we can now run APEX via `apex.py` launcher:

```
python apex.py main.bc main.c 16
```

`apex.py` requires three arguments. See usage for full description:

``` bash
usage: apex.py [-h] [--export EXPORT] code file line

positional arguments:
  code             C source code compiled into LLVM bytecode.
  file             Target file name (NOT FULL PATH).
  line             Target line number.

optional arguments:
  -h, --help       show this help message and exit
  --export EXPORT  true/false for exporting call graphs.
```

APEX produces extracted executable called `extracted` along with the `build`
directory (containing various files, logs, etc.).


### Current limitations:

Since APEX is under development, there are currently some serious limitations:

- It is possible to extract values only from integer variables.
- APEX can handle small programs (see examples direcotry), but may have problems
with bigger ones.
- (Probably tons more that I don't know about.)
