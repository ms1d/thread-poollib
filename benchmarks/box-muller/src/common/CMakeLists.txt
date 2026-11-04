cmake_minimum_required(VERSION 3.26)

add_library(box-muller-common STATIC
	common.cpp
)

target_link_libraries(box-muller-common PUBLIC
    thread-poollib
)

target_include_directories(box-muller-common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(box-muller-common PUBLIC
    -O3 -g
)

target_compile_definitions(box-muller-common PRIVATE
    NUM_TASKS=${NUM_TASKS}
    NUM_THREADS=${NUM_THREADS}
	VEC_SIZE=${VEC_SIZE}
)
