#include <cstdint>
#include <cstdio>
#include <cstdlib>

#ifndef VEC_SIZE
#define VEC_SIZE 10'000'000
#endif

void calculate(const uint32_t *vec1, const uint32_t *vec2, uint32_t *vec3, const uint32_t len) {
	for (uint32_t i = 0; i < len; i++) vec3[i] = vec1[i] + vec2[i];
}

int main() {
	uint32_t *vec1 = (uint32_t*)malloc(VEC_SIZE * sizeof(uint32_t));
	uint32_t *vec2 = (uint32_t*)malloc(VEC_SIZE * sizeof(uint32_t));
	uint32_t *vec3 = (uint32_t*)malloc(VEC_SIZE * sizeof(uint32_t));

	for (uint32_t i = 0; i < VEC_SIZE; i++) {
		vec1[i] = i * 123456789u + 12345u;
		vec2[i] = i * 987654321u + 54321u;
	}

	calculate(vec1, vec2, vec3, VEC_SIZE);

	uint32_t sum = 0;
	for (uint32_t i = 0; i < VEC_SIZE; i++) sum += vec3[i];
	std::printf("%d\n", sum);

	free(vec3);
	free(vec2);
	free(vec1);

	return 0;
}

