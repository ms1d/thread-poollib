# Benchmarks

All benchmark source files are under this directory and can be included with the
CMake option `SET_BENCHMARKS` which defaults to `OFF`

## Implemented Benchmarks

- `vector-add` (vector addition)

## Planned Benchmarks

> see issue #7

## Directory Structure

For benchmark `my-bench`, it should contain:

1. A `README.md` for description + benchmark results
2. A `CMakeLists.txt` for build instructions
3. A `launch.sh` script for running the benchmark
4. 1 directory for each `pool_type` (including single threaded execution)

Each sub-directory for each `pool_type` should contain a `main.cpp` source file
and a `CMakeLists.txt` for further build instructions.

## `ctor-dtor`

This is a special benchmark that should be referenced by other benchmarks to take
into account the overhead of the thread pool constructors and destructors. In certain
applications it may be possible to eliminate this overhead per operation by re-using
the same thread pool (e.g. in a long running process). However, for full transparency,
the total runtimes should always be shown.
