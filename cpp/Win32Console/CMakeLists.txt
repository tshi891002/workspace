cmake_minimum_required(VERSION 3.10)
project(PingPongWin32Console LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(pingpong pingpong.cpp)

if (WIN32)
    target_compile_definitions(pingpong PRIVATE UNICODE _UNICODE)
endif()
