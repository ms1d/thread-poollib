cmake_minimum_required(VERSION 3.26)

add_executable(ctor-dtor-vyukov-spin main.cpp)
target_link_libraries(ctor-dtor-vyukov-spin PRIVATE thread-poollib)
target_compile_options(ctor-dtor-vyukov-spin PRIVATE
	-O3 -g
)

target_compile_definitions(ctor-dtor-vyukov-spin PRIVATE
	NUM_TASKS=${NUM_TASKS}
	NUM_THREADS=${NUM_THREADS}
	VEC_SIZE=${VEC_SIZE}
)
