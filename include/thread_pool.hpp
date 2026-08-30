#pragma once

#include "mpmc.hpp"
#include <atomic>
#include <thread>
#include <cstdint>
#include <type_traits>
#include <tuple>
#include <cassert>


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

	tp_task() = default;
	tp_task(arg_Ts... _args) : args(std::make_tuple(_args...)) { }
};


// Specialization for tasks that return void.
template<typename... arg_Ts, void (*func)(arg_Ts...)>
struct tp_task<func> {
    std::atomic<bool> is_result_ready{false};  // Atomic boolean flag indicating if the result is ready
    std::tuple<arg_Ts...> args;  // Tuple containing the arguments for the task
	
	tp_task() = default;
	tp_task(arg_Ts... _args) : args(std::make_tuple(_args...)) { }
};



// MPMC thread pool with constexpr sizes for worker and task buffers.
// Offers 4 different pool types (see pool_type enum) all with identical APIs.
// Default pool type is mutex.
template<auto func, uint32_t worker_buffer_len, uint32_t task_buffer_len, pool_type pool_type = pool_type::mutex>
class thread_pool;

// (pool_type::mutex) Mutex-protected buffer implementation of thread pool.
// - 1 mutex protects the whole buffer; consumers and producers cannot work at the same time
// - Usually worse throughput, but do benchmark in your applications
//
// (pool_type::vyukov*) Lock free implementation of thread pool based on Vyukov's sequence numbers
//	- Each producer/consumer will increment tail/head pointers to claim slots from other threads of their "role" respectively
//	- They then spin on the newly reserved resource to allow a new consumer/producer to finish writing/reading data from it
template<typename R, typename... arg_Ts, R (*func)(arg_Ts...), uint32_t worker_buffer_len, uint32_t task_buffer_len, pool_type type>
requires(type == pool_type::vyukov_spin || type == pool_type::vyukov_idle || type == pool_type::mutex)
class thread_pool<func, worker_buffer_len, task_buffer_len, type> {



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
		task_buffer.shutdown();

        for (uint32_t i = 0; i < worker_buffer_len; i++)
            worker_buffer[i].join();
    }


	// Blocking method that submits a task to the thread pool, and waits if it is full.
	// Callers are responsible for keeping `task` alive. Stack allocating is not recommended.
	bool submit(tp_task<func> *task) {
		return task_buffer.submit(task);
	}

    // One-shot version of `submit()`. Returns false instead of waiting.
    bool try_submit(tp_task<func> *task) {
		return task_buffer.try_submit(task);
    }

    // Claims a task from the thread pool and executes it.
    bool claim() {
		tp_task<func> *task = task_buffer.claim();
		if (task == nullptr) return false;

        if constexpr (!std::is_void_v<R>) task->result = std::apply(func, task->args);
        else std::apply(func, task->args);

        task->is_result_ready.store(true, std::memory_order_release);
        task->is_result_ready.notify_one();

        return true;
    }

    // One-shot version of `claim()`. Returns false instead of waiting.
    bool try_claim() {
        tp_task<func> *task = task_buffer.try_claim();
		if (task == nullptr) return false;

        if constexpr (!std::is_void_v<R>) task->result = std::apply(func, task->args);
        else std::apply(func, task->args);

        task->is_result_ready.store(true, std::memory_order_release);
        task->is_result_ready.notify_one();

        return true;
    }


private:
    std::thread worker_buffer[worker_buffer_len];
	mpmc<tp_task<func>, task_buffer_len, type> task_buffer;


    void worker_loop() {
        while (claim());
    }
};
