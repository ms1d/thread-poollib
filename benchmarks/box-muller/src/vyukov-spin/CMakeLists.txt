cmake_minimum_required(VERSION 3.26)

add_executable(box-muller-vyukov-spin main.cpp)
target_link_libraries(box-muller-vyukov-spin PRIVATE
	box-muller-common
)

target_compile_options(box-muller-vyukov-spin PRIVATE
	-O3
)
