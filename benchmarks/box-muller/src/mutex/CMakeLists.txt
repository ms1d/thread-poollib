cmake_minimum_required(VERSION 3.26)

add_executable(box-muller-mutex main.cpp)
target_link_libraries(box-muller-mutex PRIVATE
	box-muller-common
)
