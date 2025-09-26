#!/bin/bash

set -e

# Build with clint tool clang-tidy
CC=clang CXX=clang++ cmake \
    -DCMAKE_CXX_CLANG_TIDY="clang-tidy;-warnings-as-errors=*;-header-filter=$(realpath ..)" \
    ..

# "-k" : continue as much as possible after an error
make -k
