cmake_minimum_required(VERSION 3.26)

add_executable(ctor-dtor-work-stealing main.cpp)
target_link_libraries(ctor-dtor-work-stealing PRIVATE thread-poollib)
target_compile_options(ctor-dtor-work-stealing PRIVATE -O3)
