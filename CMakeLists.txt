cmake_minimum_required(VERSION 3.14)
project(socket_prog)

set(CMAKE_CXX_STANDARD 17)

add_executable(server server.cpp server_utils.h server_utils.cpp utils.cpp utils.h)
add_executable(client client.cpp client_utils.cpp client_utils.h utils.cpp utils.h)