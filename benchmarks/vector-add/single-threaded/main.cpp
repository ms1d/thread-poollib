#define VEC_SIZE 1'000'000

void calculate(const int *vec1, const int *vec2, int *vec3, const int len) {
	for (int i = 0; i < len; i++) *vec3 = vec1[i] + vec2[i];
}

int main() {
	int *vec1 = new int[VEC_SIZE];
	int *vec2 = new int[VEC_SIZE];
	int *vec3 = new int[VEC_SIZE];

	calculate(vec1, vec2, vec3, VEC_SIZE);

	delete[] vec1;
    delete[] vec2;
    delete[] vec3;

	return 0;
}

