#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <cstdint>
#include <type_traits>
#include <tuple>
#include <cassert>

#define MAX_WORKERS 16
#define MAX_TASKS 1024


// Template struct representing a task.
template<auto func>
struct tp_task;

// Specialization for tasks that return a value.
template<typename R, typename... arg_Ts, R (*func)(arg_Ts...)>
requires(!std::is_void_v<R>)
struct tp_task<func> {
    R result;          // Var to store the result of the task
    std::atomic<bool> is_result_ready{false};  // Atomic boolean flag indicating if the result is ready
    std::tuple<arg_Ts...> args;  // Tuple containing the arguments for the task

	tp_task<func>(arg_Ts... _args) : args(std::make_tuple(_args...)) { }
};


// Specialization for tasks that return void.
template<typename... arg_Ts, void (*func)(arg_Ts...)>
struct tp_task<func> {
    std::atomic<bool> is_result_ready{false};  // Atomic boolean flag indicating if the result is ready
    std::tuple<arg_Ts...> args;  // Tuple containing the arguments for the task
	
	tp_task<func>(arg_Ts... _args) : args(std::make_tuple(_args...)) { }
};


// Enumerates different types of thread pool buffers.
enum pool_type {
    mutex_protected_buffer = 0,  // Single mutex protects the entire buffer
    vyukov_style_buffer = 1,     // Vyukov-style two-pointer + sequence number approach, lock-free
    reserving_vyukov_buffer = 2, // Similar to above but with less busy waiting and more aggressive reserving
    work_stealing_buffer = 3   // Typical decentralized work-stealing approach
};


// MPMC thread pool with constexpr sizes for worker and task buffers.
// Offers 4 different pool types (see pool_type enum) all with identical APIs.
// Default pool type is mutex_protected_buffer.
template<auto func, uint32_t worker_buffer_len, uint32_t task_buffer_len, pool_type pool_type = pool_type::mutex_protected_buffer>
class thread_pool;

// Mutex-protected buffer implementation of thread pool.
// - 1 mutex protects the whole buffer; consumers and producers cannot work at the same time
// - Usually worse throughput due to this, but do benchmark in your applications
template<typename R, typename... arg_Ts, R (*func)(arg_Ts...), uint32_t worker_buffer_len, uint32_t task_buffer_len>
class thread_pool<func, worker_buffer_len, task_buffer_len, pool_type::mutex_protected_buffer> {

    static_assert(worker_buffer_len <= MAX_WORKERS, "Max worker buffer size breached (see constant MAX_WORKERS)");
    static_assert(task_buffer_len <= MAX_TASKS, "Max task buffer size breached (see constant MAX_TASKS)");
    static_assert(worker_buffer_len <= task_buffer_len, "Why have more workers than tasks?");

public:
    // Constructor initializes the thread pool with worker threads.
    thread_pool() {
        for (uint32_t i = 0; i < worker_buffer_len; i++)
            worker_buffer[i] = std::thread([this] () {
                worker_loop();
            });
    }

    // Destructor stops all worker threads and joins them.
    ~thread_pool() {
        stop = true;
        task_buffer_cv.notify_all();

        for (uint32_t i = 0; i < worker_buffer_len; i++)
            worker_buffer[i].join();
    }


	// Blocking method that submits a task to the thread pool, and waits if it is full.
	// Callers are responsible for keeping `task` alive. Stack allocating is not recommended.
	bool submit(tp_task<func> *task) {

		std::unique_lock<std::mutex> l(task_buffer_mutex);
		task_buffer_cv.wait(l, [this] () {
			return stop || tail - head < task_buffer_len;
		});

		if (stop) return false;

		task_buffer[tail % task_buffer_len] = task;
		tail++;

		l.unlock();
		task_buffer_cv.notify_one();
    
        return true;
	}

    // One-shot version of `submit()`. Returns false instead of waiting.
    bool try_submit(tp_task<func> *task) {

        if (tail.load(std::memory_order_relaxed) - head.load(std::memory_order_relaxed) == task_buffer_len) return false;

        {
            std::lock_guard<std::mutex> l(task_buffer_mutex);

            if (tail - head == task_buffer_len) return false;

            task_buffer[tail % task_buffer_len] = task;
            tail++;
        }
        
		task_buffer_cv.notify_one();

        return true;
    }

    // Claims a task from the thread pool and executes it.
    bool claim() {
        tp_task<func> *task = nullptr;
        std::unique_lock<std::mutex> l(task_buffer_mutex);
        task_buffer_cv.wait(l, [this] () {
            return stop || tail.load(std::memory_order_relaxed) - head.load(std::memory_order_relaxed) != 0;
        });

        if (stop) return false;

        task = task_buffer[head % task_buffer_len];
        head++;
        l.unlock();

        if constexpr (!std::is_void_v<R>) task->result = std::apply(func, task->args);
        else std::apply(func, task->args);

        task->is_result_ready.store(true);
        task->is_result_ready.notify_one();

        return true;
    }

    // One-shot version of `claim()`. Returns false instead of waiting.
    bool try_claim() {
        tp_task<func> *task = nullptr;

        {
            std::lock_guard<std::mutex> l(task_buffer_mutex);

            if (tail.load(std::memory_order_relaxed) - head.load(std::memory_order_relaxed) == 0) return false;

            task = task_buffer[head % task_buffer_len];
            head++;
        }

        if constexpr (!std::is_void_v<R>) task->result = std::apply(func, task->args);
        else std::apply(func, task->args);

        task->is_result_ready.store(true);
        task->is_result_ready.notify_one();

        return true;
    }


private:
    std::mutex task_buffer_mutex;  // Mutex to protect the task buffer
    std::condition_variable task_buffer_cv;  // Condition variable for task availability
    tp_task<func> *task_buffer[task_buffer_len];  // Array of task pointers
    std::thread worker_buffer[worker_buffer_len];  // Array of worker threads

    std::atomic<uint32_t> head, tail;  // Atomic indices for managing the task buffer
    std::atomic<bool> stop;  // Flag to initiate shutdown of workers


    void worker_loop() {
        while (claim());
    }
};


// NOT IMPLEMENTED
template<typename R, typename... arg_Ts, R (*func)(arg_Ts...), uint32_t worker_buffer_len, uint32_t task_buffer_len>
class thread_pool<func, worker_buffer_len, task_buffer_len, pool_type::vyukov_style_buffer>;

// NOT IMPLEMENTED
template<typename R, typename... arg_Ts, R (*func)(arg_Ts...), uint32_t worker_buffer_len, uint32_t task_buffer_len>
class thread_pool<func, worker_buffer_len, task_buffer_len, pool_type::reserving_vyukov_buffer>;

// NOT IMPLEMENTED
template<typename R, typename... arg_Ts, R (*func)(arg_Ts...), uint32_t worker_buffer_len, uint32_t task_buffer_len>
class thread_pool<func, worker_buffer_len, task_buffer_len, pool_type::work_stealing_buffer>;
