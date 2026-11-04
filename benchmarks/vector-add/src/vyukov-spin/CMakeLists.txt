cmake_minimum_required(VERSION 3.26)

add_executable(vector-add-vyukov-spin main.cpp)
target_link_libraries(vector-add-vyukov-spin PRIVATE
	vector-add-common
)
