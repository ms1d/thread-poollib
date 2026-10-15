cmake_minimum_required(VERSION 3.26)

add_executable(ctor-dtor-work-stealing main.cpp)
target_link_libraries(ctor-dtor-work-stealing PRIVATE thread-poollib)
target_compile_options(ctor-dtor-work-stealing PRIVATE
	-O3 -g
)

target_compile_definitions(ctor-dtor-work-stealing PRIVATE
	NUM_TASKS=${NUM_TASKS}
	NUM_THREADS=${NUM_THREADS}
	VEC_SIZE=${VEC_SIZE}
)
