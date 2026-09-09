cmake_minimum_required(VERSION 3.26)

add_executable(vector-add-single-threaded main.cpp)
target_link_libraries(vector-add-single-threaded PRIVATE thread-poollib)
target_compile_options(vector-add-single-threaded PRIVATE -O3)
