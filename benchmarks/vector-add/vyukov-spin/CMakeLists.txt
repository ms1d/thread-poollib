cmake_minimum_required(VERSION 3.26)

add_executable(vector-add-vyukov-spin main.cpp)
target_link_libraries(vector-add-vyukov-spin PRIVATE thread-poollib)
target_compile_options(vector-add-vyukov-spin PRIVATE -O3)
