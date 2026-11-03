cmake_minimum_required(VERSION 3.26)

add_executable(vector-add-single-threaded main.cpp)
target_link_libraries(vector-add-single-threaded PRIVATE thread-poollib)
target_compile_options(vector-add-single-threaded PRIVATE
	-O3 -g
)

target_compile_definitions(vector-add-single-threaded PRIVATE
	NUM_TASKS=${NUM_TASKS}
	NUM_THREADS=${NUM_THREADS}
	VEC_SIZE=${VEC_SIZE}
)
