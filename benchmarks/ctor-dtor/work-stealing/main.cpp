#include "thread_pool.hpp"

void func(int) { }

int main() {
	thread_pool<func, 16, 8192, pool_type::work_stealing> pool{};
}
