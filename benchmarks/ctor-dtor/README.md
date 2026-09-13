# Benchmark: `ctor-dtor`

## Overview

This is a special benchmark that should be referenced by other benchmarks to take
into account the overhead of the thread pool constructors and destructors. In certain
applications it may be possible to eliminate this overhead per operation by re-using
the same thread pool (e.g. in a long running process). However, for full transparency,
the total runtimes should always be shown.

## Results

| Name            | Avg. Time ± S.D. (ms) |
| --------------- | --------------------- |
| Mutex           | 1.5 ± 0.3             |
| Vyukov (idle)   | 1.6 ± 0.3             |
| Vyukov (spin)   | 1.6 ± 0.3             |
| Work-Stealing   | 1.7 ± 0.4             |
