cmake_minimum_required(VERSION 3.26)

add_executable(vector-add-single-threaded main.cpp)
target_link_libraries(vector-add-single-threaded PRIVATE
	vector-add-common
)

target_compile_definitions(vector-add-single-threaded PRIVATE
	NUM_TASKS=${NUM_TASKS}
	NUM_THREADS=${NUM_THREADS}
	VEC_SIZE=${VEC_SIZE}
)
