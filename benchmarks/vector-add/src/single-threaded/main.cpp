#include <cstdio>

#ifndef VEC_SIZE
#define VEC_SIZE 10'000'000
#endif

void calculate(const int *vec1, const int *vec2, int *vec3, const int len) {
	for (int i = 0; i < len; i++) vec3[i] = vec1[i] + vec2[i];
}

int main() {
	int *vec1 = new int[VEC_SIZE];
	int *vec2 = new int[VEC_SIZE];
	int *vec3 = new int[VEC_SIZE];

	calculate(vec1, vec2, vec3, VEC_SIZE);

	int sum = 0;
	for (int i = 0; i < VEC_SIZE; i++) sum += vec3[i];
	std::printf("%d\n", sum);

	delete[] vec1;
    delete[] vec2;
    delete[] vec3;

	return 0;
}

