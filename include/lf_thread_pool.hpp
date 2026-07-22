#pragma once



#include "task_t.hpp"
#include <atomic>
#include <thread>
#include <cstdint>
#include <type_traits>



#define TASK_BUFFER_MAX 128
#define THREAD_BUFFER_MAX 32



struct emplace_result {
	uint32_t index;
	bool success;

	emplace_result(uint32_t index, bool success) : index(index), success(success) {}

	static emplace_result fail() { return emplace_result(0, false); }
};



template <auto func, uint32_t task_buffer_len, uint32_t thread_buffer_len>
class lf_thread_pool;



// Ultra-Lightweight lock-free alternative to thread_pool
// Queue is bounded, so you cannot endlessly emplace tasks (only try_emplace_task)
// Accepts compile-time sizes for task and thread buffers only via template params
// Will hopefully replace thread_pool in future
template <typename R, typename... arg_Ts, R(*func)(arg_Ts...), uint32_t task_buffer_len, uint32_t thread_buffer_len>
class lf_thread_pool<func, task_buffer_len, thread_buffer_len> {



	static_assert(task_buffer_len <= TASK_BUFFER_MAX);
	static_assert(thread_buffer_len <= THREAD_BUFFER_MAX);



	public:

		lf_thread_pool() {
			for (uint32_t i = 0; i < thread_buffer_len; i++)
				thread_buffer[i] = std::thread([this] { worker_loop(); });
		}

		~lf_thread_pool() {
			stop = true;
			stop.notify_all();

			for (uint32_t i = 0; i < thread_buffer_len; i++)
				thread_buffer[i].join();
        }



		// returns the index of the newly created task, or a fail struct
		emplace_result try_emplace_task(arg_Ts... args) {
			if (stop.load(std::memory_order_relaxed)) return emplace_result::fail();
			auto head_local = head.load(std::memory_order_acquire),
				 tail_local = tail.load(std::memory_order_acquire);

			if (tail_local - head_local == task_buffer_len
					|| task_buffer[tail_local % task_buffer_len].state != task_state::empty)
				return emplace_result::fail();
			while (!tail.compare_exchange_weak(tail_local, tail_local + 1, std::memory_order_relaxed, std::memory_order_relaxed)) {
				head_local = head.load(std::memory_order_acquire),
				tail_local = tail.load(std::memory_order_acquire);

				if (tail_local - head_local == task_buffer_len
						|| task_buffer[tail_local % task_buffer_len].state != task_state::empty)
					return emplace_result::fail();
			}

			// tail_local is now reserved
			auto &task = task_buffer[tail_local % task_buffer_len];

			task.args = std::move(std::tuple<arg_Ts...>(args...));

			task.state.store(task_state::ready_for_work, std::memory_order_release);

			// wake up 1 worker waiting for tail to move
			tail.notify_one();
			// wake up said worker waiting for task to be ready
			task.state.notify_one();

			return emplace_result(tail_local, true);
		}


		R await_result(uint32_t index) {
			// assumes index is in range between head and tail
			auto &task = task_buffer[index % task_buffer_len];
			task.state.wait(task_state::ready_for_work, std::memory_order_acquire);

			if constexpr(!std::is_void_v<R>) {
				task.state.store(task_state::empty, std::memory_order_relaxed);
				task.state.notify_one();
				return task.result;
			} else {
				task.state.store(task_state::empty, std::memory_order_relaxed);
				task.state.notify_one();
			}
		
		}



	private:
		void worker_loop() {
			while (!stop) {
				auto head_local = head.load(std::memory_order_acquire),
					 tail_local = tail.load(std::memory_order_acquire);

				while (head_local != tail_local &&
						!head.compare_exchange_weak(
							head_local, head_local + 1,
							std::memory_order_relaxed, std::memory_order_relaxed)) {

					head_local = head.load(std::memory_order_acquire),
					tail_local = tail.load(std::memory_order_acquire);
				}

				if (head_local == tail_local) {
					tail.wait(tail_local, std::memory_order_acquire);
					continue;
				}

				auto &task = task_buffer[head_local % task_buffer_len];
				if (task.state != task_state::ready_for_work) task.state.wait(task_state::empty, std::memory_order_acquire);


				// head_local is now reserved and ready for work - do work
				task.execute();
			}
		}


		// Convention: threads consume from head, clients produce at tail
		std::atomic<uint32_t> head, tail;
		task_t<func> task_buffer[task_buffer_len];
		std::thread thread_buffer[thread_buffer_len];
		
		std::atomic<bool> stop = false;
};

// TODO - add specialisation when task buffer len = thread buffer len since optimisations can be made
