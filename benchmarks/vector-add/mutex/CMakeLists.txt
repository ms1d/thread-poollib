cmake_minimum_required(VERSION 3.26)

add_executable(vector-add-mutex main.cpp)
target_link_libraries(vector-add-mutex PRIVATE thread-poollib)
target_compile_options(vector-add-mutex PRIVATE -O3)
