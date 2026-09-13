# Benchmark: `vector-add`

## Overview

Addition of 2 integer vectors, each 1 million elements long.
Each thread pool contains 16 threads and 8192 tasks in its task buffer.

## Results

All benchmarks were run on a Ryzen 7 8845HS (8C/16T where appropiate).

### Raw

| Name            | Avg. Time ± S.D. (ms) |
| --------------- | --------------------- |
| Single-Threaded | 1.0 ± 0.4             |
| Mutex           | 2.4 ± 0.4             |
| Vyukov (idle)   | 2.5 ± 0.4             |
| Vyukov (spin)   | 2.3 ± 0.4             |
| Work-Stealing   | 2.2 ± 0.6             |

### Adjusted

Using these values from ctor-dtor (correct at the time of writing):

| Name            | Avg. Time ± S.D. (ms) |
| --------------- | --------------------- |
| Mutex           | 1.5 ± 0.3             |
| Vyukov (idle)   | 1.6 ± 0.3             |
| Vyukov (spin)   | 1.6 ± 0.3             |
| Work-Stealing   | 1.7 ± 0.4             |

The adjusted times are as follows:

| Name            | Avg. Time ± S.D. (ms) |
| --------------- | --------------------- |
| Single-Threaded | 1.0 ± 0.4             |
| Mutex           | 0.9 ± 0.4             |
| Vyukov (idle)   | 0.9 ± 0.4             |
| Vyukov (spin)   | 0.7 ± 0.4             |
| Work-Stealing   | 0.5 ± 0.6             |

## Conclusions

Overall, this demonstrates that niavely introducing concurrency through the thread
pool implementations is ineffective for simple vector addition. The overhead of
`thread_pool`'s ctor and dtor are significantly greater than the overhead of the
actual performed task, and as such it is not recommended to create a new pool per
operation. The overhead of task scheduling and synchronisation is also significant,
showing that using 16 times as many threads can only lead upto a ~2x increase in
throughput. Thread pools may perform better on more irregular workloads where the
CPU has greater difficulty with branch prediction, cache locality, or prefetching.
GPU parallelisation is likely to outperform the CPU due to the sheer volume of
computation it provides, but this is outside the scope of this project.
