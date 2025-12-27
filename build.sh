#!/bin/bash

set -xe

mkdir -p build
cd build

cmake -S .. -B . \
    -D CMAKE_EXPORT_COMPILE_COMMANDS=1 \
    -D CMAKE_CXX_COMPILER=clang++

cmake --build . -j $(nproc)



