#include "common.hpp"
#include <cstdio>
#include <cstdlib>

void calculate(const uint32_t *vec1, const uint32_t *vec2, uint32_t *vec3, const uint32_t len) {
	for (uint32_t i = 0; i < len; i++) vec3[i] = vec1[i] + vec2[i];
}

bool vec_ptrs::alloc_vecs() {
	vec1 = (uint32_t*)malloc(VEC_SIZE * sizeof(uint32_t));
	vec2 = (uint32_t*)malloc(VEC_SIZE * sizeof(uint32_t));
	vec3 = (uint32_t*)malloc(VEC_SIZE * sizeof(uint32_t));

	return (vec1 != NULL && vec2 != NULL && vec3 != NULL);
}

void vec_ptrs::init_vecs() {
	for (uint32_t i = 0; i < VEC_SIZE; i++) {
		vec1[i] = i * 123456789u + 12345u;
        vec2[i] = i * 987654321u + 54321u;
	}
}

void vec_ptrs::free_vecs() {
    free(vec3);
    free(vec2);
    free(vec1);
}

template<pool_type type>
void execute_main_body() {
	vec_ptrs ptrs{};
	auto &vec1 = ptrs.vec1,
		 &vec2 = ptrs.vec2,
		 &vec3 = ptrs.vec3;

	if (!ptrs.alloc_vecs()) exit(1);
	ptrs.init_vecs();

	thread_pool<calculate, NUM_THREADS, NUM_TASKS, type> pool{};

	tp_task<calculate> *tasks[NUM_THREADS];
	for (uint32_t i = 0; i < NUM_THREADS; i++) {
		auto offset = i * ELES_PER_THREAD;
		tasks[i] = new tp_task<calculate>(
				vec1 + offset,
				vec2 + offset,
				vec3 + offset,
				ELES_PER_THREAD);
	}

	for (uint32_t i = 0; i < NUM_THREADS; i++) {
		pool.submit(tasks[i]);
	}

	for (uint32_t i = 0; i < NUM_THREADS; i++) {
		tasks[i]->is_result_ready.wait(false);
        delete tasks[i];
    }

	uint32_t sum = 0;
	for (uint32_t i = 0; i < VEC_SIZE; i++) sum += vec3[i];
	std::printf("%d\n", sum);
	
	ptrs.free_vecs();
}

template void execute_main_body<pool_type::mutex>();
template void execute_main_body<pool_type::vyukov_spin>();
template void execute_main_body<pool_type::vyukov_idle>();
template void execute_main_body<pool_type::work_stealing>();
