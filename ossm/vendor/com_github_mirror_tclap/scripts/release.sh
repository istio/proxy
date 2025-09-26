#!/bin/bash

set -x
set -e

bash autotools.sh
mkdir -p build
cd build
../configure
make -j8
make -C docs manual
make -j8 distcheck
