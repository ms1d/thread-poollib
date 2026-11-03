cmake_minimum_required(VERSION 3.26)

add_library(vector-add-common STATIC
    common.cpp
)

target_link_libraries(vector-add-common PUBLIC
    thread-poollib
)

target_include_directories(vector-add-common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
