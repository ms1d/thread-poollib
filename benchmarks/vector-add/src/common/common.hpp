#pragma once

#include "thread_pool.hpp"
#include <cstdint>

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

static_assert(VEC_SIZE < std::numeric_limits<uint32_t>::max());
static_assert(VEC_SIZE > std::numeric_limits<uint32_t>::min());
static_assert(NUM_TASKS < std::numeric_limits<uint32_t>::max());
static_assert(NUM_TASKS < std::numeric_limits<uint32_t>::max());
static_assert(NUM_THREADS > std::numeric_limits<uint32_t>::min());
static_assert(NUM_THREADS > std::numeric_limits<uint32_t>::min());

void calculate(const uint32_t *vec1, const uint32_t *vec2, uint32_t *vec3, const uint32_t len);

struct vec_ptrs {
	uint32_t *vec1, *vec2, *vec3;

	bool alloc_vecs();

	void init_vecs();

	void free_vecs();
};

// Intended for thread pool benchmarks
template<pool_type type>
void execute_main_body();
