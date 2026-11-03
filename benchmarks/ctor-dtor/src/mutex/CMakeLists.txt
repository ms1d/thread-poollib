cmake_minimum_required(VERSION 3.26)

add_executable(ctor-dtor-mutex main.cpp)
target_link_libraries(ctor-dtor-mutex PRIVATE thread-poollib)
target_compile_options(ctor-dtor-mutex PRIVATE 
	-O3 -g
)

target_compile_definitions(ctor-dtor-mutex PRIVATE
	NUM_TASKS=${NUM_TASKS}
	NUM_THREADS=${NUM_THREADS}
	VEC_SIZE=${VEC_SIZE}
)
