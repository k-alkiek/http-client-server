cmake_minimum_required(VERSION 3.14)
project(socket_prog)

set(CMAKE_CXX_STANDARD 17)

add_executable(server server.cpp server_util.cpp server_util.h)
add_executable(client client.cpp)