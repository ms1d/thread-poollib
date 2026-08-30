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



static thread_local void *deque_ptr = nullptr;
// Chase-Lev style work stealing thread pool implementation. Each worker has its own deque
// External submit and claim go to the induction buffer, NOT a particular worker deque
template<typename R, typename... arg_Ts, R (*func)(arg_Ts...), uint32_t worker_buffer_len, uint32_t task_buffer_len>
class thread_pool<func, worker_buffer_len, task_buffer_len, pool_type::work_stealing> {


public:

	thread_pool() {
		for (uint32_t i = 0; i < worker_buffer_len; i++) {
			workers[i] = std::thread([this, i] () {
				worker_loop(i);
			});
		}
	}

	~thread_pool() {
		stop = true;
		induction_epoch.notify_all();
		induction_buffer.shutdown();
		for (uint32_t i = 0; i < worker_buffer_len; i++) workers[i].join();
	}

	bool submit(tp_task<func> *task) {
		if (deque_ptr != nullptr) {
			deque *q = (deque*)deque_ptr;
			auto res = q->push(task);
			if (res) return true;
		}

		if (induction_buffer.submit(task)) {
			induction_epoch.fetch_add(1, std::memory_order_relaxed);
			induction_epoch.notify_one();
			return true;
		}

		return false;
	}

	bool try_submit(tp_task<func> *task) {
		if (deque_ptr != nullptr) {
			deque *q = (deque*)deque_ptr;
			auto res = q->push(task);
			if (res) return true;
		}

		if (induction_buffer.try_submit(task)) {
			induction_epoch.fetch_add(1, std::memory_order_relaxed);
			induction_epoch.notify_one();
			return true;
		}

		return false;
	}

	bool claim() {
		tp_task<func> *task = nullptr;
		if (deque_ptr != nullptr) task = ((deque*)deque_ptr)->pop();
		if (task == nullptr) task = induction_buffer.claim();

		if (task != nullptr) {
            if constexpr (!std::is_void_v<R>) task->result = std::apply(func, task->args);
            else std::apply(func, task->args);
            task->is_result_ready.store(true, std::memory_order_release);
            task->is_result_ready.notify_one();
            return true;
		}

		return false;
	}

	bool try_claim() {
		tp_task<func> *task = nullptr;
		if (deque_ptr != nullptr) task = ((deque*)deque_ptr)->pop();
		if (task == nullptr) task = induction_buffer.try_claim();

		if (task != nullptr) {
            if constexpr (!std::is_void_v<R>) task->result = std::apply(func, task->args);
            else std::apply(func, task->args);
            task->is_result_ready.store(true, std::memory_order_release);
            task->is_result_ready.notify_one();
            return true;
		}

		return false;
	}


private:

	struct deque {
		// convention: owner moves top, thieves steal from bottom
		alignas(64) std::atomic<uint32_t> top, bottom;
		tp_task<func> *task_buffer[task_buffer_len];

		bool push(tp_task<func> *task) {
			auto top_local = top.load(std::memory_order_relaxed),
				 bottom_local = bottom.load(std::memory_order_relaxed);
			if (top_local - bottom_local == task_buffer_len) return false;
			task_buffer[top_local % task_buffer_len] = task;
			top.store(top_local + 1, std::memory_order_release);
			top.notify_one();
			return true;
		}

		tp_task<func> *pop() {
			for(;;) {
				auto top_local = top.load(std::memory_order_relaxed),
					 bottom_local = bottom.load(std::memory_order_relaxed);

				if (top_local == bottom_local) return nullptr;

				if (top_local == bottom_local + 1) {
					if (!bottom.compare_exchange_strong(bottom_local, bottom_local + 1, std::memory_order_relaxed, std::memory_order_relaxed)) return nullptr;
					return task_buffer[bottom_local % task_buffer_len];

				} else {
					if (!top.compare_exchange_strong(top_local, top_local - 1, std::memory_order_relaxed, std::memory_order_relaxed)) continue;
					return task_buffer[top_local % task_buffer_len];
				}
			}
		}

		tp_task<func> *steal() {
			auto top_local = top.load(std::memory_order_acquire),
				 bottom_local = bottom.load(std::memory_order_relaxed);

			while (top_local != bottom_local && !bottom.compare_exchange_weak(bottom_local, bottom_local + 1, std::memory_order_acquire, std::memory_order_relaxed)) {
				top_local = top.load(std::memory_order_acquire);
			}

			if (top_local == bottom_local) return nullptr;

			return task_buffer[bottom_local % task_buffer_len];
		}

		uint32_t size() {
			auto top_local = top.load(std::memory_order_relaxed),
				 bottom_local = bottom.load(std::memory_order_relaxed);
			return top_local - bottom_local;
		}
	};

	deque deques[worker_buffer_len];

	mpmc<tp_task<func>, task_buffer_len, pool_type::vyukov_idle> induction_buffer;
	alignas(64) std::atomic<uint32_t> induction_epoch;

	std::thread workers[worker_buffer_len];
	std::atomic<bool> stop;

	void worker_loop(uint32_t worker_index) {
		deque_ptr = deques + worker_index;

		for (;;) {
			if (deques[worker_index].size() == 0 && stop.load(std::memory_order_relaxed)) return;

			if (auto task = deques[worker_index].pop()) {
				if constexpr (!std::is_void_v<R>) task->result = std::apply(func, task->args);
				else std::apply(func, task->args);
				task->is_result_ready.store(true, std::memory_order_release);
				task->is_result_ready.notify_one();
				continue;
			}

			tp_task<func> *task = nullptr;
			for (uint32_t i = 0; i < worker_buffer_len; i++) {
				if ((task = deques[i].steal()) != nullptr) break;
			}

			if (task != nullptr) {
                if constexpr (!std::is_void_v<R>) task->result = std::apply(func, task->args);
                else std::apply(func, task->args);
                task->is_result_ready.store(true, std::memory_order_release);
                task->is_result_ready.notify_one();
                continue;
            }

			if (!try_claim()) {
				auto local_epoch = induction_epoch.load(std::memory_order_relaxed);
				induction_epoch.wait(local_epoch, std::memory_order_relaxed);
			}
		}
	}

};
