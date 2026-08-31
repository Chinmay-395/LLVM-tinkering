# Flow-sensitive points-to analysis

`SimplePointsTo.cpp` is an LLVM 14 new-pass-manager plugin implementing a small,
intraprocedural, forward, flow-sensitive may-points-to analysis. It computes a
CFG fixed point and models:

- stack objects introduced by `alloca`;
- pointer-valued `load` and `store` instructions;
- strong updates for a single known destination;
- weak updates for multiple possible destinations.

The abstract state deliberately separates the locations named by pointer SSA
values from the pointer values stored in memory:

```text
valuePts[pointer SSA value] = {abstract stack locations}
memoryPts[stack location]   = {abstract stack locations stored there}
```

This is a teaching baseline, not a sound analysis for arbitrary C. It does not
model calls, heap allocations, GEPs/aggregate fields, globals, casts, function
pointers, or integer/pointer conversions.

## Requirements

- LLVM and Clang 14
- a C++17 compiler (the Make default is the system `g++`/`c++`)
- Bash (for the regression suite)

The tool names can be overridden for another LLVM installation:

```bash
make LLVM_CONFIG=llvm-config-14 CXX=g++
```

## Build and run

```bash
make
clang -O0 -fno-discard-value-names -emit-llvm -c test.c -o test.bc
opt -load-pass-plugin=./build/SimplePointsTo.so \
  -passes=simple-pta -disable-output test.bc
```

The pass prints both maps at every basic-block `OUT` state. Output entries are
sorted by their LLVM operand names, making runs and regression checks stable.

Run all focused examples with:

```bash
make test
```
