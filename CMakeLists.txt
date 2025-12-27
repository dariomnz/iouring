cmake_minimum_required(VERSION 3.15)
project(UringEchoServer CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_library(LIBURING_LIB uring REQUIRED)

file(GLOB_RECURSE HEADERS 
    "src/**.hpp"
)

file(GLOB_RECURSE SOURCES
    "src/**.cpp"
)

add_executable(uring_echo ${HEADERS} ${SOURCES})

target_link_libraries(uring_echo ${LIBURING_LIB} pthread)

target_compile_options(uring_echo PRIVATE -Wall -Wextra)
# target_compile_options(uring_echo PRIVATE -DDEBUG)