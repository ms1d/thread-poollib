cmake_minimum_required(VERSION 3.26)

add_executable(vector-add-work-stealing main.cpp)
target_link_libraries(vector-add-work-stealing PRIVATE thread-poollib)
target_compile_options(vector-add-work-stealing PRIVATE -O3)

