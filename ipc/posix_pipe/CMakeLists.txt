cmake_minimum_required(VERSION 3.10)
project(posix_pipe_example)

set(CMAKE_CXX_STANDARD 17)

add_executable(reader ipc_reader.cpp )
add_executable(writer ipc_writer.cpp )