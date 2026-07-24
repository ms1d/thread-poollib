#pragma once



#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <cstdint>
#include <type_traits>
#include <tuple>
#include <tuple>
#include <cassert>

#define MAX_WORKERS 16
#define MAX_TASKS 1024



template<auto func>
struct tp_task;


template<typename R, typename... arg_Ts, R (*func)(arg_Ts...)> requires(!std::is_void_v<R>)
struct tp_task<func> {
	R *result;
	std::atomic<bool> *is_result_ready;
	std::tuple<arg_Ts...> args;
};


// Special case when tasks return void
template<typename... arg_Ts, void (*func)(arg_Ts...)>
struct tp_task<func> {
	std::atomic<bool> *is_result_ready;
	std::tuple<arg_Ts...> args;
};



enum pool_type {
	mutex_protected_buffer = 0, // 1 mutex protects the whole buffer
	vyukov_style_buffer = 1, // vyokov-styled 2 pointer + sequence number approach, lock free
	reserving_vyukov_buffer = 2, // similar to above, but with less busy waiting and more aggressive reserving
	work_stealing_buffer = 3 // typical decentralised work stealing approach
};



// MPMC thread pool with constexpr sizes for worker and task buffers
// Offers 4 different pool types (see pool_type enum) all with identical APIs
// Default pool type is mutex_protected_buffer
template<auto func, uint32_t worker_buffer_len, uint32_t task_buffer_len, pool_type pool_type = pool_type::mutex_protected_buffer>
class thread_pool;


// Mutex protected buffer implementation of thread pool
//	- 1 mutex protects the whole buffer; consumers and producers cannot work at the same time
//	- Usually worse throughput due to this, but do benchmark in your applications
template<typename R, typename... arg_Ts, R (*func)(arg_Ts...), uint32_t worker_buffer_len, uint32_t task_buffer_len>
class thread_pool<func, worker_buffer_len, task_buffer_len, pool_type::mutex_protected_buffer> {



	static_assert(worker_buffer_len <= MAX_WORKERS, "Max worker buffer size breached (see constant MAX_WORKERS)");
	static_assert(task_buffer_len <= MAX_TASKS, "Max task buffer size breached (see constant MAX_TASKS)");
	static_assert(worker_buffer_len <= task_buffer_len, "Why have more workers than tasks?");



	public:

	thread_pool() {
		for (uint32_t i = 0; i < worker_buffer_len; i++)
			worker_buffer[i] = std::thread([this] () {
				worker_loop();
			});
	}

	~thread_pool() {
		stop = true;
		task_buffer_cv.notify_all();

		for (uint32_t i = 0; i < worker_buffer_len; i++)
            worker_buffer[i].join();
	}


	// Attempts to emplace a task in the task buffer. Returns success state (queued or not)
	// Requires:
	//	- R* to store results to (unless void)
	//	- std::atomic<bool>* to notify clients when results are ready
	//	- arg_Ts... args
	// all bundled in a tp_task object (see tp_task.hpp)
	// This object MUST persist during task completion. Callers are responsible for NOT losing it
	bool try_submit(tp_task<func> *task) {
		// basic input sanitisation
		if constexpr (!std::is_void_v<R>) if (task->result == nullptr) return false;
		
		if (task->is_result_ready == nullptr) return false;

		if (tail.load(std::memory_order_relaxed) - head.load(std::memory_order_relaxed) == task_buffer_len) return false;

		{
			std::lock_guard<std::mutex> l(task_buffer_mutex);

			// This thread may have been pre empted before the lock has been created, so check again and return if necessary
			if (tail.load(std::memory_order_relaxed) - head.load(std::memory_order_relaxed) == task_buffer_len) return false;

			// Now this thread has control over tail
			task_buffer[tail % task_buffer_len] = task;

			// Increment tail
			tail++;

			task_buffer_cv.notify_one();
		}

		return true;
	}

	// Returns whether a task has been completed or not
	// Useful for producers to help out if waiting for results
	bool claim() {
		tp_task<func> *task = nullptr;
		std::unique_lock<std::mutex> l(task_buffer_mutex);
		task_buffer_cv.wait(l, [this] () {
			return stop || tail.load() - head.load() != 0;
		});

		if (stop) return false;

		// Worker now reserves queue at head
		task = task_buffer[head % task_buffer_len];
		head++;
		l.unlock();

		if constexpr (!std::is_void_v<R>) task->result = std::apply(func, task->args);
		else std::apply(func, task->args);

		task->is_result_ready->store(true);
		task->is_result_ready->notify_one();

		return true;
	}

	// One-shot version of the above
	bool try_claim() {
		tp_task<func> *task = nullptr;

		{
			std::lock_guard<std::mutex> l(task_buffer_mutex);

			if (tail - head == 0) return false;

			// Worker now reserves queue at head
			task = task_buffer[head % task_buffer_len];
			head++;
		}

		if constexpr (!std::is_void_v<R>) task->result = std::apply(func, task->args);
		else std::apply(func, task->args);

		task->is_result_ready->store(true);
		task->is_result_ready->notify_one();

		return true;
	}



	private:

	std::mutex task_buffer_mutex;
	std::condition_variable task_buffer_cv;
	tp_task<func> *task_buffer[task_buffer_len];
	std::thread worker_buffer[worker_buffer_len];

	// Head is the next index to be consumed, tail is the next index to be produced in
	std::atomic<uint32_t> head, tail;

	std::atomic<bool> stop; // Flag that initiates shutdown of workers

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
