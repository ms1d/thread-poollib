# thread-poollib

Lightweight templated MPMC thread pool header-only library designed to get
out of your way when you need MPMC concurrency.

## `pool_type`

- `mutex_protected_buffer` protects the buffer with a mutex

- `vyukov_style_buffer` draws inspiration from Vyukov's design on 1024cores

- `reserving_vyukov_buffer` is similar to the above but with some slight changes
to make producers more aggressive when reserving slots to allow for efficient waiting

- `work_stealing_buffer` uses a completely distinct, decentralised
paradigm when enqueueing tasks

If you have better names for these types please do let me know.
As of writing, only the first type has been implemented.

## Usage

Say I need to compute, concurrently:

```cpp
int foo(const int *num1, const int *num2) {
    *output = *input * *input;
}
```

I instantiate the pool as

```cpp
#define THREADS 4
#define SLOTS 16

thread_pool<foo, THREADS, SLOTS> bar{}; // Defaults type to pool_type::mutex_protected_buffer
```

The constructor obviously creates threads; avoid in a hot loop!

To queue tasks:

```cpp
tp_task<foo> task{&user_input_from_somewhere, &user_input_from_somewhere_else};

bar.submit(&task); // Blocks, only returns false if the pool is shutting down

task.is_result_ready.wait(false); // Wait patiently

// Now you can access task.result! This member does
// not exist for void returning functions
```

Of course, pointers are not strictly necessary but are natural to use here.

If you are enqueuing MANY tasks from MANY threads, including workers (in
recursive contexts), feel free to use bar.claim() or bar.try_claim() to help out!

```cpp
// Use acquire to ensure task is fully written to when we read it
while (!task.is_result_ready.load(std::memory_order_acquire)) {
    if (!bar.try_claim()) task.is_result_ready.wait(false);
}
```

## Notes

- Thread and Task buffers are statically sized at compile time

- YOU, the caller, are responsible for ensuring that task objects
passed in remain alive while workers compute them.

- If using this pool in a recursive context, I would recommend
heap allocating tasks to avoid stack use after returns caused by stack re-use.
