cmake_minimum_required(VERSION 3.26)

add_executable(ctor-dtor-vyukov-spin main.cpp)
target_link_libraries(ctor-dtor-vyukov-spin PRIVATE thread-poollib)
target_compile_options(ctor-dtor-vyukov-spin PRIVATE -O3)
