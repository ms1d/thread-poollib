cmake_minimum_required(VERSION 3.26)

add_executable(vector-add-mutex main.cpp)
target_link_libraries(vector-add-mutex PRIVATE thread-poollib)
target_compile_options(vector-add-mutex PRIVATE
	-O3 -g
)

target_compile_definitions(vector-add-mutex PRIVATE
	NUM_TASKS=${NUM_TASKS}
	NUM_THREADS=${NUM_THREADS}
	VEC_SIZE=${VEC_SIZE}
)
