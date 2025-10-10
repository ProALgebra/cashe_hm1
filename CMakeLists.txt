cmake_minimum_required(VERSION 3.10)
project(cache-benchmark LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)



add_executable(L1D_assoc
        L1D_assoc.cpp)

add_executable(L1D_cache_size
        L1d_cache_size.cpp)

add_executable(cache_line_size cache_line_size.cpp)
