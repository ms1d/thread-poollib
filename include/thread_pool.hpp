#pragma once



#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <vector>
#include "task_t.hpp"



template<auto func>
class thread_pool;

// Simple, lightweight bounded thread pool.
// To init, provide number of workers + function to run with arg_Ts input
// Use try_emplace_task() to add a task. if it returns false, the task could not be added
template<typename R, typename... arg_Ts, R(*func)(arg_Ts...)>
class thread_pool<func> {
	public:
		thread_pool(const size_t worker_count) : worker_count(worker_count) {

			if (worker_count == 0)
				throw std::runtime_error("worker_count must be > 0");

			workers.reserve(worker_count);

			for (size_t i = 0; i < worker_count; i++) {
				workers.emplace_back([this] { worker_loop(); });
			}
		}
		
		bool try_emplace_task(std::atomic<bool> *is_ready = nullptr, arg_Ts... args) {
			if (stop.load(std::memory_order_relaxed)) return false;
			auto curr = busy_workers.fetch_add(1, std::memory_order_relaxed);
			if (curr >= worker_count) {
				busy_workers.fetch_sub(1, std::memory_order_relaxed);
				return false;
			}
		
			emplace_task_internal(is_ready, args...);

			return true;
		}

		void emplace_task(std::atomic<bool> *is_ready = nullptr, arg_Ts... args) {
			if (stop.load(std::memory_order_relaxed)) return;
			emplace_task_internal(is_ready, args...);
		}

		~thread_pool() {
			stop = true;
			tasks_cv.notify_all();
			for (auto &worker : workers) worker.join();
		}



	private:
		std::condition_variable tasks_cv;
		std::queue<task_t<R, arg_Ts...>> tasks;
		std::mutex tasks_mtx;

		std::vector<std::thread> workers;

		std::atomic<uint> busy_workers = 0;

		const size_t worker_count;

		// Flag for all threads to READ to determine whether or not they should take on more tasks
		std::atomic<bool> stop = false;

		void emplace_task_internal(std::atomic<bool> *is_ready, arg_Ts... args) {
			task_t<R, arg_Ts...> task {
				std::tuple<arg_Ts...>(args...),
				is_ready
			};

			{
				std::lock_guard<std::mutex> lock(tasks_mtx);
				tasks.push(std::move(task));
			}

			tasks_cv.notify_one();
		}
		
		void worker_loop() {
			while (1) {
				std::unique_lock<std::mutex> lock(tasks_mtx);

				tasks_cv.wait(lock, [this] () {
					return stop || !tasks.empty();
				});

				if (stop && tasks.empty()) break;

				auto t = std::move(tasks.front());
				
				tasks.pop();

				lock.unlock();

				t.execute(func);

				busy_workers--;
			}
		}



};
