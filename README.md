# thread-poollib

Lightweight templated MPMC thread pool header-only library designed to get
out of your way when you need MPMC concurrency.

## Features

- Supports 4 pool types (see below)

- **Templated** & **Header-only** for easy usage in projects

- **Compile-time function binding** and **fixed-size buffers** to minimise latency

- Includable via **CMake FetchContent** for easy dependency management

## `pool_type`

- `mutex_protected_buffer` protects the buffer with a mutex

- `vyukov_buffer_spin` draws inspiration from Dimitri Vyukov's design on [1024cores](https://sites.google.com/site/1024cores/home/lock-free-algorithms/queues/bounded-mpmc-queue)

- `vyukov_buffer_idle` where Vyukov spins, idle instead with std::atomic::wait()

- (WIP) `work_stealing_buffer` uses a completely distinct, decentralised
paradigm when enqueueing tasks

As of writing, only the first 3 types have been implemented.

## Architecture

diagram go here

## Usage

Consider the function:

```cpp
int foo(const int *num1, const int *num2) {
    return *num1 * *num2;
}
```

The pool should be instantiated as

```cpp
// Sample values
#define THREADS 16
#define SLOTS 1024 

thread_pool<foo, THREADS, SLOTS> bar{}; // Defaults type to pool_type::mutex_protected_buffer
```

Avoid constantly constructing and destroying pools as it creates and destroys threads.

To queue tasks:

```cpp
tp_task<foo> task{&input_1, &input_2};

bar.submit(&task); // Blocks, only returns false if the pool is shutting down

task.is_result_ready.wait(false);

// task.result is now available for non-void returning functions
```

In recursive contexts, `try_claim` is available for users to avoid deadlocks:

```cpp
// Use acquire to ensure task is fully written to when we read it
while (!task.is_result_ready.load(std::memory_order_acquire)) {
    if (!bar.try_claim()) task.is_result_ready.wait(false);
}
```

### `wrapper` pattern

To use multiple functions for the thread pool, wrap them in a small wrapper function:

```cpp
inline int wrapper(int type, void *data) {
    switch (type) {
        case FUNC_1_TYPE: {
            auto args = static_cast<func_1_args*>(data);
            return func_1(args);
            break;
        }
        case FUNC_2_TYPE: {
            auto args = static_cast<func_2_args*>(data);
            return func_2(args);
            break;
        }
        default:
            assert(false && "Invalid wrapper type");
    }
}
```

With `func_N_args` being structs that help map each byte of `void *data` to each
argument expected. This obviously adds a small overhead per function call but is
typically negligible.

## Benchmarks

> A benchmark suite is to be added soon. See issue #7

## Notes, Limitations & Quirks

- Thread and Task buffers are statically sized at compile time

- YOU, the caller, are **responsible** for ensuring that task objects
passed in **remain alive** while workers compute them.

- If using this pool in a recursive context, it is recommended to
heap allocate tasks. Stack use after returns can be caused by stack re-use.

- Dynamically sized buffers and worker pools are **not** supported, but
may be added in a future patch.

- Tasks **cannot** be cancelled.

- Exception safety is **not** supported.
