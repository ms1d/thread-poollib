#pragma once



#include <atomic>
#include <tuple>



enum task_state {
	empty = 0,
	ready_for_work = 1,
	ready_for_result = 2
};



template<auto func>
struct task_t;



// structs for tasks used in thread_pools
template<typename R, typename... arg_Ts, R (*func)(arg_Ts...)> requires (!std::is_void_v<R>)
struct task_t<func> {
	std::tuple<arg_Ts...> args;
	std::atomic<task_state> state;
	R result;

	task_t() {
		args = std::tuple<arg_Ts...>();
		state = task_state::empty;
	}

	void execute() {
		result = std::apply(func, args);

		state.store(task_state::ready_for_result, std::memory_order_release);					
		state.notify_one();
	}
};


template<typename... arg_Ts, void (*func)(arg_Ts...)> 
struct task_t<func> {
	std::tuple<arg_Ts...> args;
	std::atomic<task_state> state;

	task_t() {
		args = std::tuple<arg_Ts...>();
		state = task_state::empty;
	}

	void execute() {
		std::apply(func, args);

		state.store(task_state::ready_for_result, std::memory_order_release);					
		state.notify_one();
	}
};
