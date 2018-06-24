# APEX - Active code Path EXtractor

APEX is a tool that can extract active code path from the C program and recompile
this path into new, standalone executable.

### Getting started

In order to run APEX, you need to have llvm installed:
- https://llvm.org/docs/GettingStarted.html
- https://releases.llvm.org/3.8.0/docs/CMake.html




### Current limitations:
```
- supports only one compilation module
- handles only void returning functions on the active path
- does not handle external libraries
- (probably tons more that I don't know about)
```

