cmake_minimum_required(VERSION 3.26)

add_executable(box-muller-vyukov-idle main.cpp)
target_link_libraries(box-muller-vyukov-idle PRIVATE
	box-muller-common
)
