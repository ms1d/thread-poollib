#pragma once



#include <atomic>
#include <tuple>



// structs for tasks used in thread_pools
template<typename R, typename... arg_Ts>
struct task_t {
	std::tuple<arg_Ts...> args;
	std::atomic<bool> *is_ready;
	R result;

	void execute(R(*func)(arg_Ts...)){
		result = std::apply(func, args);

		if (is_ready != nullptr) {	
			is_ready->store(true);					
			is_ready->notify_one();
		}
	}
};


template<typename... arg_Ts>
struct task_t<void, arg_Ts...> {
	std::tuple<arg_Ts...> args;
	std::atomic<bool> *is_ready;

	void execute(void(*func)(arg_Ts...)){	
		std::apply(func, args);

		if (is_ready != nullptr) {
			is_ready->store(true);
			is_ready->notify_one();
		}
	}
};
