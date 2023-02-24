cmake_minimum_required(VERSION 3.22.1)
project(net_tools)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)
aux_source_directory(src SRC)
file(GLOB_RECURSE HEADER "include/*.h")

add_executable(net_tools src/main.cpp ${SRC} ${HEADER})
