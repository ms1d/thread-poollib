cmake_minimum_required(VERSION 3.26)

add_executable(box-muller-work-stealing main.cpp)
target_link_libraries(box-muller-work-stealing PRIVATE
	box-muller-common
)

target_compile_options(box-muller-work-stealing PRIVATE
	-O3
)
