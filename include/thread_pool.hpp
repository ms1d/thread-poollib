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


// Enumerates different types of thread pool buffers.
enum pool_type {
    mutex_protected_buffer = 0,		// Single mutex protects the entire buffer
    vyukov_buffer_spin = 1,			// Vyukov-style two-pointer + sequence number approach, lock-free
    vyukov_buffer_idle = 2,			// Similar to above but threads idle instead of spinning
    work_stealing_buffer = 3		// Typical decentralized work-stealing approach
};


// MPMC thread pool with constexpr sizes for worker and task buffers.
// Offers 4 different pool types (see pool_type enum) all with identical APIs.
// Default pool type is mutex_protected_buffer.
template<auto func, uint32_t worker_buffer_len, uint32_t task_buffer_len, pool_type pool_type = pool_type::mutex_protected_buffer>
class thread_pool;

// Mutex-protected buffer implementation of thread pool.
// - 1 mutex protects the whole buffer; consumers and producers cannot work at the same time
// - Usually worse throughput, but do benchmark in your applications
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
			return tail - head < task_buffer_len || stop.load(std::memory_order_relaxed);
		});

		if (stop.load(std::memory_order_relaxed)) return false;

		task_buffer[tail % task_buffer_len] = task;
		tail++;

		l.unlock();
		task_buffer_cv.notify_one();
    
        return true;
	}

    // One-shot version of `submit()`. Returns false instead of waiting.
    bool try_submit(tp_task<func> *task) {
        {
            std::lock_guard<std::mutex> l(task_buffer_mutex);

			if (tail.load(std::memory_order_relaxed) - head.load(std::memory_order_relaxed) == task_buffer_len
					|| stop.load(std::memory_order_relaxed))
				return false;

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
            return stop || tail.load(std::memory_order_relaxed) != head.load(std::memory_order_relaxed);
        });

        task = task_buffer[head % task_buffer_len];
        head++;
        l.unlock();

        if constexpr (!std::is_void_v<R>) task->result = std::apply(func, task->args);
        else std::apply(func, task->args);

        task->is_result_ready.store(true, std::memory_order_release);
        task->is_result_ready.notify_one();
        
		if (stop.load(std::memory_order_relaxed)
				&& tail.load(std::memory_order_relaxed) == head.load(std::memory_order_relaxed)) return false;

        return true;
    }

    // One-shot version of `claim()`. Returns false instead of waiting.
    bool try_claim() {
		if (stop.load(std::memory_order_relaxed)) return false;
        tp_task<func> *task = nullptr;

        {
            std::lock_guard<std::mutex> l(task_buffer_mutex);

            if (tail.load(std::memory_order_relaxed) - head.load(std::memory_order_relaxed) == 0) return false;

            task = task_buffer[head % task_buffer_len];
            head++;
        }

        if constexpr (!std::is_void_v<R>) task->result = std::apply(func, task->args);
        else std::apply(func, task->args);

        task->is_result_ready.store(true, std::memory_order_release);
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


// Lock free implementation of thread pool via Vyukov sequence numbers
//	- Each producer/consumer will increment tail/head pointers to claim slots from other threads of their "role" respectively
//	- They then spin on the newly reserved resource to allow a new consumer/producer to finish writing/reading data from it
template<typename R, typename... arg_Ts, R (*func)(arg_Ts...), uint32_t worker_buffer_len, uint32_t task_buffer_len, pool_type type>
//requires(type == pool_type::vyukov_buffer_spin || type == pool_type::vyukov_buffer_idle)
class thread_pool<func, worker_buffer_len, task_buffer_len, type> {
public:
	
	thread_pool() {
		for (uint32_t i = 0; i < task_buffer_len; i++) 
			task_buffer[i].seq_num = i;

		for (uint32_t i = 0; i < worker_buffer_len; i++)
			worker_buffer[i] = std::thread([this] () {
                worker_loop();
            });
	}

	~thread_pool() {
		stop.store(true, std::memory_order_relaxed);
		stop.notify_all();

		for (uint32_t i = 0; i < worker_buffer_len; i++)
            worker_buffer[i].join();
	}

	// Blocking method that submits a task to the thread pool, and waits if it is full.
	// Callers are responsible for keeping `task` alive. Stack allocating is not recommended.
	bool submit(tp_task<func> *task) {
		for (;;) {
			if (stop.load(std::memory_order_relaxed)) return false;

			auto head_local = head.load(std::memory_order_relaxed),
				 tail_local = tail.load(std::memory_order_relaxed);

			while (tail_local - head_local < task_buffer_len &&
					!tail.compare_exchange_weak(tail_local, tail_local + 1, std::memory_order_relaxed, std::memory_order_relaxed)) {
				head_local = head.load(std::memory_order_relaxed);
			}

			if (tail_local - head_local >= task_buffer_len) continue;

			slot *s = &task_buffer[tail_local % task_buffer_len];
			uint32_t seq_num_local;
			while ((seq_num_local = s->seq_num.load(std::memory_order_acquire))  != tail_local) {
				if constexpr (type == pool_type::vyukov_buffer_idle) s->seq_num.wait(seq_num_local, std::memory_order_relaxed);
			}

			s->task = task;
			s->seq_num.store(tail_local + 1, std::memory_order_release);
			tail.notify_one();
			if constexpr (type == pool_type::vyukov_buffer_idle) s->seq_num.notify_one();

			return true;
		}
	}
	
    // One-shot version of `submit()`. Returns false instead of waiting if pool is full.
	bool try_submit(tp_task<func> *task) { 
		if (stop.load(std::memory_order_relaxed)) return false;
		
		auto head_local = head.load(std::memory_order_relaxed),
			 tail_local = tail.load(std::memory_order_relaxed);

		while (tail_local - head_local < task_buffer_len &&
				!tail.compare_exchange_weak(tail_local, tail_local + 1, std::memory_order_relaxed, std::memory_order_relaxed)) {
			head_local = head.load(std::memory_order_relaxed);
		}

		if (tail_local - head_local >= task_buffer_len) return false;

		slot *s = &task_buffer[tail_local % task_buffer_len];
		uint32_t seq_num_local;
		while ((seq_num_local = s->seq_num.load(std::memory_order_acquire))  != tail_local) {
			if constexpr (type == pool_type::vyukov_buffer_idle) s->seq_num.wait(seq_num_local, std::memory_order_relaxed);
		}

		s->task = task;
		s->seq_num.store(tail_local + 1, std::memory_order_release);
		tail.notify_one();
		if constexpr (type == pool_type::vyukov_buffer_idle) s->seq_num.notify_one();

		return true;
	}

    
    // Claims a task from the thread pool and executes it. Returns whether or not a task was completed
	bool claim() {
		for (;;) {	
			auto head_local = head.load(std::memory_order_relaxed),
				 tail_local = tail.load(std::memory_order_relaxed);

			while (head_local != tail_local &&
					!head.compare_exchange_weak(head_local, head_local + 1, std::memory_order_relaxed, std::memory_order_relaxed)) {
				tail_local = tail.load(std::memory_order_relaxed);
			}

			if (tail_local == head_local) {
				tail.wait(tail_local, std::memory_order_relaxed); continue;
			}


			slot *s = &task_buffer[head_local % task_buffer_len];
			uint32_t seq_num_local;

			while ((seq_num_local = s->seq_num.load(std::memory_order_acquire)) != head_local + 1) {
				if constexpr (type == pool_type::vyukov_buffer_idle) s->seq_num.wait(seq_num_local, std::memory_order_relaxed);
			}

			tp_task<func> *task = s->task;
			s->seq_num.store(seq_num_local + task_buffer_len, std::memory_order_release);
			head.notify_one();
			if constexpr (type == pool_type::vyukov_buffer_idle) s->seq_num.notify_one();

			if constexpr (!std::is_void_v<R>) task->result = std::apply(func, task->args);
			else std::apply(func, task->args);

			task->is_result_ready.store(true, std::memory_order_release);
			task->is_result_ready.notify_one();
			
			if (stop.load(std::memory_order_relaxed)
					&& tail.load(std::memory_order_relaxed) == head.load(std::memory_order_relaxed)) return false;

			return true;
		}
	}
    
    // One-shot version of `claim()`. Returns false instead of waiting.
	bool try_claim() {
		if (stop.load(std::memory_order_relaxed)) return false;

		auto head_local = head.load(std::memory_order_relaxed),
             tail_local = tail.load(std::memory_order_relaxed);

		while (head_local != tail_local
				&& !head.compare_exchange_weak(head_local, head_local + 1, std::memory_order_relaxed, std::memory_order_relaxed)) {
            tail_local = tail.load(std::memory_order_relaxed);
        }

		if (tail_local == head_local) return false;


		slot *s = &task_buffer[head_local % task_buffer_len];
		uint32_t seq_num_local;

		while ((seq_num_local = s->seq_num.load(std::memory_order_acquire)) != head_local + 1) {
			if constexpr (type == pool_type::vyukov_buffer_idle) s->seq_num.wait(seq_num_local, std::memory_order_relaxed);
		}

		tp_task<func> *task = s->task;
		s->seq_num.store(seq_num_local + task_buffer_len, std::memory_order_release);
		head.notify_one();
		if constexpr (type == pool_type::vyukov_buffer_idle) s->seq_num.notify_one();

		if constexpr (!std::is_void_v<R>) task->result = std::apply(func, task->args);
		else std::apply(func, task->args);

		task->is_result_ready.store(true, std::memory_order_release);
		task->is_result_ready.notify_one();

		return true;
	}

private:
	struct slot {
		tp_task<func> *task = nullptr;
		std::atomic<uint32_t> seq_num = 0;
	};

	slot task_buffer[task_buffer_len];
	std::thread worker_buffer[worker_buffer_len];

	std::atomic<uint32_t> head, tail;
	std::atomic<bool> stop;

	void worker_loop() {
		while (claim());
	}
};



// NOT IMPLEMENTED
template<typename R, typename... arg_Ts, R (*func)(arg_Ts...), uint32_t worker_buffer_len, uint32_t task_buffer_len>
class thread_pool<func, worker_buffer_len, task_buffer_len, pool_type::work_stealing_buffer>;
