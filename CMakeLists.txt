cmake_minimum_required(VERSION 3.10)

# c++23作为标准
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
project(coro-cpp-20)

include_directories(include)

# 添加可执行文件时需要指定目标名称和源文件
add_executable(coro-cpp-20 main.cpp)

target_link_libraries(coro-cpp-20 pthread)
