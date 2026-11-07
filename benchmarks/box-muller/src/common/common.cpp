#include "common.hpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>

void calculate(const float *vec1, const float *vec2, float *vec3, const uint32_t len) {
	constexpr float eps = 1e-6f;
	constexpr uint8_t inner_len = 2;

	for (uint32_t i = 0; i < len; i++) {
		// Easy way to ensure x and y are between 0 and 1
		auto x = std::sin(vec1[i]);
		auto y = std::cos(vec2[i]);
		x *= x; y *= y;

		for (int j = 0; j < inner_len; j++) {
			x = std::max(x, eps); y = std::max(y, eps);

			auto X = std::sqrt(-2.0f * std::log(x)) / y,
				 Y = std::sqrt(-2.0f * std::log(y)) / x;

			x = std::sin(X), x *= x;
			y = std::sin(Y), y *= y;
		}

		vec3[i] = x + y;
	}
}

bool vec_ptrs::alloc_vecs() {
	vec1 = (float*)malloc(VEC_SIZE * sizeof(float));
	vec2 = (float*)malloc(VEC_SIZE * sizeof(float));
	vec3 = (float*)malloc(VEC_SIZE * sizeof(float));

	return (vec1 != NULL && vec2 != NULL && vec3 != NULL);
}

void vec_ptrs::init_vecs() {
	for (uint32_t i = 0; i < VEC_SIZE; i++) {
		// time is used as a source of pseudorandomness
		auto t = std::chrono::high_resolution_clock::now().time_since_epoch().count();
		vec1[i] = i * 123456789u + 12345u * t;
        vec2[i] = i * 987654321u + 54321u * t;
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
	thread_pool<calculate, NUM_THREADS, NUM_TASKS, type> pool{};
	tp_task<calculate> *tasks[NUM_THREADS];

	for (uint32_t i = 0; i < INNER_LOOPS; i++) {
		ptrs.init_vecs();

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
	}
	
	ptrs.free_vecs();
}

template void execute_main_body<pool_type::mutex>();
template void execute_main_body<pool_type::vyukov_spin>();
template void execute_main_body<pool_type::vyukov_idle>();
template void execute_main_body<pool_type::work_stealing>();
