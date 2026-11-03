cmake_minimum_required(VERSION 3.26)

add_executable(vector-add-work-stealing main.cpp)
target_link_libraries(vector-add-work-stealing PRIVATE thread-poollib)
target_compile_options(vector-add-work-stealing PRIVATE
	-O3 -g
)

target_compile_definitions(vector-add-work-stealing PRIVATE
	NUM_TASKS=${NUM_TASKS}
	NUM_THREADS=${NUM_THREADS}
	VEC_SIZE=${VEC_SIZE}
)
