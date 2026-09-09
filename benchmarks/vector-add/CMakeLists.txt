cmake_minimum_required(VERSION 3.26)

add_subdirectory(single-threaded)
add_subdirectory(mutex)
add_subdirectory(vyukov-idle)
add_subdirectory(vyukov-spin)
add_subdirectory(work-stealing)
