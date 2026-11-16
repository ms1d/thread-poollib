cmake_minimum_required(VERSION 3.26)

add_executable(vector-add-manual main.cpp)
target_link_libraries(vector-add-manual PRIVATE
	vector-add-common
)
