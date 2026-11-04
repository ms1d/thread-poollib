cmake_minimum_required(VERSION 3.26)

add_executable(vector-add-manual main.cpp)
target_link_libraries(vector-add-manual PRIVATE
	vector-add-common
)

target_compile_definitions(vector-add-manual PRIVATE
	NUM_TASKS=${NUM_TASKS}
	NUM_THREADS=${NUM_THREADS}
	VEC_SIZE=${VEC_SIZE}
)
