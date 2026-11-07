cmake_minimum_required(VERSION 3.26)

add_executable(ctor-dtor-manual main.cpp)
target_link_libraries(ctor-dtor-manual PRIVATE thread-poollib)
target_compile_options(ctor-dtor-manual PRIVATE 
	-O3
)

target_compile_definitions(ctor-dtor-manual PRIVATE
	NUM_TASKS=${NUM_TASKS}
	NUM_THREADS=${NUM_THREADS}
	VEC_SIZE=${VEC_SIZE}
)
