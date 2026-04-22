cmake_minimum_required(VERSION 3.20)
project(TaskFlowAPI)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(TaskFlowAPI main.cpp)

target_include_directories(TaskFlowAPI PRIVATE include)
