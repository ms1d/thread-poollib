#include <cstdio>
#include <cstdlib>
#include "common.hpp"

int main() {
	vec_ptrs ptrs{};
	auto &vec1 = ptrs.vec1,
		 &vec2 = ptrs.vec2,
		 &vec3 = ptrs.vec3;

	if (!ptrs.alloc_vecs()) exit(1);
	ptrs.init_vecs();

	calculate(vec1, vec2, vec3, VEC_SIZE);

	uint32_t sum = 0;
	for (uint32_t i = 0; i < VEC_SIZE; i++) sum += vec3[i];
	std::printf("%d\n", sum);

	ptrs.free_vecs();

	return 0;
}

