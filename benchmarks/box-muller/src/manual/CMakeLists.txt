cmake_minimum_required(VERSION 3.26)

add_executable(box-muller-manual main.cpp)
target_link_libraries(box-muller-manual PRIVATE
	box-muller-common
)

target_compile_definitions(box-muller-manual PRIVATE
    NUM_TASKS=${NUM_TASKS}
    NUM_THREADS=${NUM_THREADS}
	VEC_SIZE=${VEC_SIZE}
)
