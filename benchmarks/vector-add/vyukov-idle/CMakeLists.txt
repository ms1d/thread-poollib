cmake_minimum_required(VERSION 3.26)

add_executable(vector-add-vyukov-idle main.cpp)
target_link_libraries(vector-add-vyukov-idle PRIVATE thread-poollib)
target_compile_options(vector-add-vyukov-idle PRIVATE -O3)
