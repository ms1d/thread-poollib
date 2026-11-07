cmake_minimum_required(VERSION 3.26)

set(VEC_SIZE 10000000 CACHE STRING "Number of elements in each vector")
set(INNER_LOOPS 100 CACHE STRING "Number of repeats per run")

add_subdirectory(src)
