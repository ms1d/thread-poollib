#include <cstdint>
#include <cstdio>

#ifndef VEC_SIZE
#define VEC_SIZE 10'000'000
#endif

void calculate(const uint32_t *vec1, const uint32_t *vec2, uint32_t *vec3, const uint32_t len) {
	for (uint32_t i = 0; i < len; i++) vec3[i] = vec1[i] + vec2[i];
}

int main() {
	uint32_t *vec1 = new uint32_t[VEC_SIZE];
	uint32_t *vec2 = new uint32_t[VEC_SIZE];
	uint32_t *vec3 = new uint32_t[VEC_SIZE];

	calculate(vec1, vec2, vec3, VEC_SIZE);

	uint32_t sum = 0;
	for (uint32_t i = 0; i < VEC_SIZE; i++) sum += vec3[i];
	std::printf("%d\n", sum);

	delete[] vec1;
    delete[] vec2;
    delete[] vec3;

	return 0;
}

