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
    R result;
    std::atomic<bool> is_result_ready{false};
    std::tuple<arg_Ts...> args;
#ifndef NDEBUG
	std::atomic_flag is_executing = ATOMIC_FLAG_INIT;
#endif

	void reset_flag() {
#ifndef NDEBUG
		is_executing.clear(std::memory_order_release);
#endif
	}

	tp_task() = default;
	tp_task(arg_Ts... _args) : args(std::make_tuple(_args...)) { }
};


// Specialization for tasks that return void.
template<typename... arg_Ts, void (*func)(arg_Ts...)>
struct tp_task<func> {
    std::atomic<bool> is_result_ready{false};
    std::tuple<arg_Ts...> args;
#ifndef NDEBUG
	std::atomic_flag is_executing = ATOMIC_FLAG_INIT;
#endif

	void reset_flag() {
#ifndef NDEBUG
		is_executing.clear(std::memory_order_release);
#endif
	}
	
	tp_task() = default;
	tp_task(arg_Ts... _args) : args(std::make_tuple(_args...)) { }
};



template<typename R, typename... arg_Ts, R (*func)(arg_Ts...)>
bool execute(tp_task<func> *task) {
	assert(!task->is_executing.test_and_set(std::memory_order_acquire));
    if constexpr (!std::is_void_v<R>) task->result = std::apply(func, task->args);
    else std::apply(func, task->args);
    task->is_result_ready.store(true, std::memory_order_release);
    task->is_result_ready.notify_one();
#ifndef NDEBUG
	task->is_executing.clear(std::memory_order_release);
#endif
	return true;
}



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
			worker_buffer[i] = std::thread([this] () { worker_loop(); });
    }

    // Destructor stops all worker threads and joins them.
    ~thread_pool() {
		while (exited != worker_buffer_len) task_buffer.shutdown();

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
		return execute(task);
    }

    // One-shot version of `claim()`. Returns false instead of waiting.
    bool try_claim() {
        tp_task<func> *task = task_buffer.try_claim();
		if (task == nullptr) return false;
		return execute(task);
    }


private:
    std::thread worker_buffer[worker_buffer_len];
	std::atomic<uint32_t> exited{0};
	mpmc<tp_task<func>, task_buffer_len, type> task_buffer;


    void worker_loop() {
        while (claim());
		exited++;
    }
};



struct thread_info {
	void *deque_ptr = nullptr, *pool_ptr = nullptr;

	bool is_worker(void *curr_pool) { return deque_ptr != nullptr && pool_ptr == curr_pool; }
};
static thread_local thread_info curr_thread{};

// wyrand (64-bit)
static std::atomic<uint32_t> seed_counter{0};
static thread_local uint64_t state = 0x9e3779b97f4a7c15ULL ^ seed_counter.fetch_add(1);

static inline uint64_t wyrand() {
    state += 0xa0761d6478bd642f;
    __uint128_t t = static_cast<__uint128_t>(state) * (state ^ 0xe7037ed1a0b428db);
    return static_cast<uint64_t>(t >> 64) ^ static_cast<uint64_t>(t);
}

// Chase-Lev style work stealing thread pool implementation. Each worker has its own deque
template<typename R, typename... arg_Ts, R (*func)(arg_Ts...), uint32_t worker_buffer_len, uint32_t task_buffer_len>
class thread_pool<func, worker_buffer_len, task_buffer_len, pool_type::work_stealing> {


public:

	thread_pool() {
		for (uint32_t i = 0; i < worker_buffer_len; i++)
			worker_buffer[i] = std::thread([this, i] () { worker_loop(i); });
	}

	thread_pool(uint32_t _max_steals) : max_steals(_max_steals) {
		for (uint32_t i = 0; i < worker_buffer_len; i++)
			worker_buffer[i] = std::thread([this, i] () { worker_loop(i); });
	}

	~thread_pool() {
		induction_buffer.shutdown();
		induction_epoch.fetch_add(1, std::memory_order_relaxed);
		induction_epoch.notify_all();
		while (exited != worker_buffer_len) induction_buffer.shutdown();
		for (uint32_t i = 0; i < worker_buffer_len; i++) worker_buffer[i].join();
	}

	// Blocking method that submits a task to the thread pool, and waits if it is full.
	// Callers are responsible for keeping `task` alive. Stack allocating is not recommended.
	bool submit(tp_task<func> *task) {
		if (curr_thread.is_worker(this)) {
			auto res = static_cast<deque*>(curr_thread.deque_ptr)->push(task);
			if (res) {
				induction_epoch.fetch_add(2, std::memory_order_relaxed);
				induction_epoch.notify_one();
				return true;
			}
		}

		if (induction_buffer.submit(task)) {
			induction_epoch.fetch_add(2, std::memory_order_relaxed);
			induction_epoch.notify_one();
			return true;
		}

		return false;
	}

    // One-shot version of `submit()`. Returns false instead of waiting.
	bool try_submit(tp_task<func> *task) {
		if (curr_thread.is_worker(this)) {
			auto res = static_cast<deque*>(curr_thread.deque_ptr)->push(task);
			if (res) {
				induction_epoch.fetch_add(2, std::memory_order_relaxed);
				induction_epoch.notify_one();
				return true;
			}
		}

		if (induction_buffer.try_submit(task)) {
			induction_epoch.fetch_add(2, std::memory_order_relaxed);
			induction_epoch.notify_one();
			return true;
		}

		return false;
	}

    // Claims a task from the thread pool and executes it.
	bool claim() {
		tp_task<func> *task = nullptr;

		if (curr_thread.is_worker(this) && (task = (static_cast<deque*>(curr_thread.deque_ptr))->pop()) != nullptr) {
			return execute(task);
		}

		uint32_t start_index = wyrand();
		for (uint32_t i = start_index; i < start_index + max_steals; i++) {
			if (deques + i == curr_thread.deque_ptr) continue;
			if ((task = deques[i % worker_buffer_len].steal()) != nullptr) return execute(task);
		}

		if ((task = induction_buffer.claim()) != nullptr) {
			return execute(task);
		}

		return false;
	}

    // One-shot version of `claim()`. Returns false instead of waiting.
	bool try_claim() {
		tp_task<func> *task = nullptr;

		if (curr_thread.is_worker(this) && (task = (static_cast<deque*>(curr_thread.deque_ptr))->pop()) != nullptr) {
			return execute(task);
		}

		uint32_t start_index = wyrand();
		for (uint32_t i = start_index; i < start_index + max_steals; i++) {
			if (deques + i == curr_thread.deque_ptr) continue;
			if ((task = deques[i % worker_buffer_len].steal()) != nullptr) return execute(task);
		}

		if ((task = induction_buffer.try_claim()) != nullptr) {
			return execute(task);
		}

		return false;
	}


private:

	struct deque {
		// convention: owner moves bottom, thieves steal from top
		std::atomic<uint32_t> top{0}, bottom{0};
		tp_task<func> *task_buffer[task_buffer_len];

		bool push(tp_task<func> *task) {
			auto top_local = top.load(std::memory_order_relaxed),
				 bottom_local = bottom.load(std::memory_order_relaxed);
			if (bottom_local - top_local == task_buffer_len) return false;
			task_buffer[bottom_local % task_buffer_len] = task;
			bottom.store(bottom_local + 1, std::memory_order_release); // release write to thieves
			return true;
		}

		tp_task<func> *pop() {
			auto bottom_local = bottom.load(std::memory_order_relaxed);
			bottom_local--;
			bottom.store(bottom_local, std::memory_order_relaxed);

			// This thread fence was used in the original Chase-Lev paper
			// It forces top_local to be newer than bottom_local
			// This prevents owners from optimistically decrementing
			// bottom when they should instead be CASing top
			std::atomic_thread_fence(std::memory_order_seq_cst);

			auto top_local = top.load(std::memory_order_relaxed);

			if (static_cast<int32_t>(bottom_local - top_local) < 0) {
				bottom.store(bottom_local + 1, std::memory_order_relaxed);
				return nullptr;
			}

			auto task = task_buffer[bottom_local % task_buffer_len];

			if (top_local == bottom_local) {
                if (!top.compare_exchange_strong(top_local, top_local + 1, std::memory_order_relaxed, std::memory_order_relaxed)) task = nullptr;
                bottom.store(bottom_local + 1, std::memory_order_relaxed);
            }
				return task;

			return nullptr;
		}

		tp_task<func> *steal() {
			auto top_local = top.load(std::memory_order_relaxed);
			auto bottom_local = bottom.load(std::memory_order_acquire); // acquire write to task_buffer from owner

			if (static_cast<int32_t>(bottom_local - top_local) <= 0) return nullptr;

			auto task = task_buffer[top_local % task_buffer_len];

			if (!top.compare_exchange_strong(top_local, top_local + 1, std::memory_order_relaxed, std::memory_order_relaxed)) {
				return nullptr;
			}

			return task;
		}

		uint32_t size() {
			auto top_local = top.load(std::memory_order_relaxed),
				 bottom_local = bottom.load(std::memory_order_relaxed);
			return bottom_local - top_local;
		}
	};

	deque deques[worker_buffer_len];

	mpmc<tp_task<func>, task_buffer_len, pool_type::vyukov_idle> induction_buffer;
	std::atomic<uint32_t> induction_epoch{0}, exited{0};

	std::thread worker_buffer[worker_buffer_len];

	const uint32_t max_steals = 2;

	void worker_loop(uint32_t worker_index) {
		curr_thread.deque_ptr = deques + worker_index;
		curr_thread.pool_ptr = this;

		for (;;) {
			if (!try_claim()) {
				auto local_epoch = induction_epoch.load(std::memory_order_relaxed);
				if (local_epoch % 2 == 1) break;
				induction_epoch.wait(local_epoch, std::memory_order_relaxed);
			}
		}

		exited++;
	}

};
