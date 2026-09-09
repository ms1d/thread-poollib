cmake_minimum_required(VERSION 3.26)

project(thread-pool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(thread-poollib INTERFACE)
target_include_directories(thread-poollib INTERFACE include)

option(BUILD_BENCHMARKS "Build benchmarks" OFF)

if (BUILD_BENCHMARKS)
	add_subdirectory(benchmarks)
endif()
