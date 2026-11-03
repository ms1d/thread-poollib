#include <thread>
#include "common.hpp"

int main() {
	vec_ptrs ptrs{};
	auto &vec1 = ptrs.vec1,
		 &vec2 = ptrs.vec2,
		 &vec3 = ptrs.vec3;

	if (!ptrs.alloc_vecs()) exit(1);
	ptrs.alloc_vecs();

	std::thread pool[NUM_THREADS];
	for (uint32_t i = 0; i < NUM_THREADS; i++) {
		pool[i] = std::thread([&, i] {
			auto offset = i * ELES_PER_THREAD;
			calculate(vec1 + offset, vec2 + offset, vec3 + offset, ELES_PER_THREAD);
		});
	}

	for (uint32_t i = 0; i < NUM_THREADS; i++) pool[i].join();
	
	uint32_t sum = 0;
	for (uint32_t i = 0; i < VEC_SIZE; i++) sum += vec3[i];
	std::printf("%d\n", sum);

	ptrs.free_vecs();

	return 0;
}
