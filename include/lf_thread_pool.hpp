#pragma once



#include "task_t.hpp"
#include <atomic>
#include <thread>



#define TASK_BUFFER_MAX 128
#define THREAD_BUFFER_MAX 32



template <auto func, uint task_buffer_len, uint thread_buffer_len>
class lf_thread_pool;



// Ultra-Lightweight lock-free alternative to thread_pool
// Queue is bounded, so you cannot endlessly emplace tasks (only try_emplace_task)
// Accepts compile-time sizes for task and thread buffers only via template params
template <typename R, typename... arg_Ts, R(*func)(arg_Ts...), uint task_buffer_len, uint thread_buffer_len>
class lf_thread_pool<func, task_buffer_len, thread_buffer_len> {



	static_assert(task_buffer_len <= TASK_BUFFER_MAX);
	static_assert(thread_buffer_len <= THREAD_BUFFER_MAX);



	public:
		lf_thread_pool() {
			for (uint i = 0; i < thread_buffer_len; i++)
				thread_buffer[i] = std::thread([this] { worker_loop(); });
		}


		~lf_thread_pool() {
			for (uint i = 0; i < thread_buffer_len; i++)
				thread_buffer[i].join();
        }



	private:
		void worker_loop() {
			while (!stop) {

			}
		}


		std::array<task_t<R, arg_Ts...>, task_buffer_len> task_buffer;
		std::array<std::thread, thread_buffer_len> thread_buffer;
		
		std::atomic<bool> stop;
};
