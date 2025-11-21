cmake_minimum_required(VERSION 3.10)

project(server_epoll LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -O2")

add_executable(server_epoll main.cpp eserver.h eserver.cpp)
