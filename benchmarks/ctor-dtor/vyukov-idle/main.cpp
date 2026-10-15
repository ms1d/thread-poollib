#include "thread_pool.hpp"

#ifndef NUM_TASKS
#define NUM_TASKS 8192
#endif

#ifndef NUM_THREADS
#define NUM_THREADS 32
#endif

void func(int) { }

int main() {
	thread_pool<func, NUM_THREADS, NUM_TASKS, pool_type::vyukov_idle> pool{};
}
