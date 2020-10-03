# LLVM JIT example with debug information

This is just a small test application to try out debugging and profiling of JIT compiled code with LLVM.


## Profiling with perf on Linux

LLVM needs to be compiled with LLVM_USE_PERF=ON, otherwise the dump files of the JIT compiled code are not created by LLVM.

Profiling steps:
1. `perf record -k 1 ./sumDebug`
2. `perf inject -j -i perf.data -o perf.data.jitted`
3. `perf report -i perf.data.jitted`

In `perf report` the generated function "sumfunc" should appear in the list of functions.
You can inspect the code in the function by pressing 'a'.


## Debugging with GDB on Linux

Debugging is supported out-of-the-box.
No special build configuration of LLVM is required.

You can set a breakpoint on the generated function "sumfunc".
It is pending as the code is not yet compiled and therefore unknown to GDB.
Run the program and it stops execution when entering the function.
The debug information is generated together with the code and maps the instructions to the API calls of LLVM.
The usual GDB commands should work, like stepping to the next statement and disassembling the function.
