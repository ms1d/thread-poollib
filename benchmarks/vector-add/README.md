# Benchmark: `vector-add`

## Overview

Addition of 2 integer vectors, each 1 million elements long.
Each thread pool contains 16 threads and 8192 tasks in its task buffer.

## Results

All benchmarks were run on a Ryzen 7 8845HS (8C/16T where appropiate).

| Name            | Avg. Time ± S.D. (ms) |
| --------------- | --------------------- |
| Single-Threaded | 1.0 ± 0.4             |
| Mutex           | 2.4 ± 0.4             |
| Vyukov (idle)   | 2.5 ± 0.4             |
| Vyukov (spin)   | 2.3 ± 0.4             |
| Work-Stealing   | 2.2 ± 0.6             |

## Conclusions

Overall, this demonstrates that introducing concurrency through the thread pool
implementations is ineffective for simple vector addition. The overhead of task
scheduling and synchronisation outweighs the benefits of parallel execution for
this regular, highly predictable workload. Thread pools may perform better on more
irregular workloads where the CPU has greater difficulty with branch prediction,
cache locality, or prefetching. GPU parallelisation is likely to outperform the
CPU due to the sheer volume of computation it provides, but this is outside the
scope of this project. Note: these times also include construction and destruction
of the thread pool objects. A benchmark of the actual work performed by the workers
is planned.
