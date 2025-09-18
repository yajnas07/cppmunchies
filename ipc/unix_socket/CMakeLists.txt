cmake_minimum_required(VERSION 3.10)
project(posix_pipe_example)

set(CMAKE_CXX_STANDARD 17)


add_executable(unix_socket_server unix_socket_server.cpp)
add_executable(unix_socket_client unix_socket_client.cpp)
