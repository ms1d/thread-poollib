cmake_minimum_required(VERSION 3.26)

add_executable(ctor-dtor-mutex main.cpp)
target_link_libraries(ctor-dtor-mutex PRIVATE thread-poollib)
target_compile_options(ctor-dtor-mutex PRIVATE -O3)
