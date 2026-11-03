cmake_minimum_required(VERSION 3.26)

add_executable(box-muller-single-threaded main.cpp)
target_link_libraries(box-muller-single-threaded PRIVATE
	box-muller-common
)

target_compile_options(box-muller-single-threaded PRIVATE
	-O3
)

target_compile_definitions(box-muller-single-threaded PRIVATE
    NUM_TASKS=${NUM_TASKS}
    NUM_THREADS=${NUM_THREADS}
	VEC_SIZE=${VEC_SIZE}
)
