#include "thread_pool.hpp"

#ifndef NUM_TASKS
#define NUM_TASKS 128*4
#endif

#ifndef NUM_THREADS
#define NUM_THREADS 4
#endif

#ifndef VEC_SIZE
#define VEC_SIZE 10'000'000
#endif

#define ELES_PER_THREAD (VEC_SIZE / NUM_THREADS)

static_assert(VEC_SIZE % NUM_THREADS == 0);

void calculate(const uint32_t *vec1, const uint32_t *vec2, uint32_t *vec3, const uint32_t len) {
	for (uint32_t i = 0; i < len; i++) vec3[i] = vec1[i] + vec2[i];
}

int main() {
	uint32_t *vec1 = new uint32_t[VEC_SIZE];
	uint32_t *vec2 = new uint32_t[VEC_SIZE];
	uint32_t *vec3 = new uint32_t[VEC_SIZE];

	thread_pool<calculate, NUM_THREADS, NUM_TASKS, pool_type::work_stealing> pool{};

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

	delete[] vec1;
    delete[] vec2;
    delete[] vec3;

	return 0;
}
