#include "common.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

int main(void) {
	vec_ptrs ptrs{};
	auto &vec1 = ptrs.vec1,
		 &vec2 = ptrs.vec2,
		 &vec3 = ptrs.vec3;

	if (!ptrs.alloc_vecs()) exit(1);

	for (uint32_t i = 0; i < INNER_LOOPS; i++) {
		ptrs.init_vecs();

		std::thread pool[NUM_THREADS];
		for (uint32_t i = 0; i < NUM_THREADS; i++) {
			pool[i] = std::thread([&, i] {
				auto offset = i * ELES_PER_THREAD;
				calculate(vec1 + offset, vec2 + offset, vec3 + offset, ELES_PER_THREAD);
			});
		}

		for (uint32_t i = 0; i < NUM_THREADS; i++) pool[i].join();

		float sum = 0;
		for (uint32_t i = 0; i < VEC_SIZE; i++) sum += vec3[i];
		printf("%f\n", sum);
	}

	ptrs.free_vecs();

	return 0;
}
