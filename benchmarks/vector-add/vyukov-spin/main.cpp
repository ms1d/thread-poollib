#include "thread_pool.hpp"

#define NUM_TASKS 8192
#define NUM_THREADS 16
#define VEC_SIZE 1'000'000
#define ELES_PER_THREAD (VEC_SIZE / NUM_THREADS)

static_assert(VEC_SIZE % NUM_THREADS == 0);

void calculate(const int *vec1, const int *vec2, int *vec3, const int len) {
	for (int i = 0; i < len; i++) *vec3 = vec1[i] + vec2[i];
}

int main() {
	int *vec1 = new int[VEC_SIZE];
	int *vec2 = new int[VEC_SIZE];
	int *vec3 = new int[VEC_SIZE];

	thread_pool<calculate, NUM_THREADS, NUM_TASKS, pool_type::vyukov_spin> pool{};

	tp_task<calculate> *tasks[NUM_THREADS];
	for (int i = 0; i < NUM_THREADS; i++) {
		auto offset = i * ELES_PER_THREAD;
		tasks[i] = new tp_task<calculate>(
				vec1 + offset,
				vec2 + offset,
				vec3 + offset,
				ELES_PER_THREAD);
	}

	for (int i = 0; i < NUM_THREADS; i++) {
		pool.submit(tasks[i]);
	}

	for (int i = 0; i < NUM_THREADS; i++) {
		tasks[i]->is_result_ready.wait(false);
        delete tasks[i];
    }

	delete[] vec1;
    delete[] vec2;
    delete[] vec3;

	return 0;
}
