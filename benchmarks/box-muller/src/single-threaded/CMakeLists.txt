cmake_minimum_required(VERSION 3.26)

add_executable(box-muller-single-threaded main.cpp)
target_link_libraries(box-muller-single-threaded PRIVATE
	box-muller-common
)
