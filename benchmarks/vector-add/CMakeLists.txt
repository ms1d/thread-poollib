cmake_minimum_required(VERSION 3.26)

set(VEC_SIZE 10000000 CACHE STRING "Number of elements in each vector")

add_subdirectory(single-threaded)
add_subdirectory(mutex)
add_subdirectory(vyukov-idle)
add_subdirectory(vyukov-spin)
add_subdirectory(work-stealing)
add_subdirectory(manual)
