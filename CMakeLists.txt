cmake_minimum_required(VERSION 3.23)
project(weak_hash_map)

set(CMAKE_CXX_STANDARD 17)

add_executable(weak_hash_map
        hash_map.h
        test.cpp)
