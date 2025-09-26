#!/bin/bash
# Copyright 2016-2019 Envoy Project Authors
# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -e

# basics
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get upgrade -y
apt-get install -y --no-install-recommends apt-utils ca-certificates
apt-get autoremove -y
apt-get clean
apt-get install -y --no-install-recommends software-properties-common apt-transport-https git wget curl pkg-config autoconf autotools-dev automake libtool cmake python zlib1g-dev

# gcc-7
apt-get install -y --no-install-recommends gcc-7 g++-7 cpp-7
export CC=gcc-7
export CXX=g++-7
export CPP=cpp-7

# get $HOME
cd

# specific version of protobufs to match the pre-compiled support libraries
git clone https://github.com/protocolbuffers/protobuf
cd protobuf
git checkout v3.9.1
git submodule update --init --recursive
./autogen.sh
./configure
make
make check
make install
cd
rm -rf protobuf

# emscripten
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk update-tags
./emsdk install 3.1.7
./emsdk activate 3.1.7
source ./emsdk_env.sh
cd

# abseil (optional)
git clone https://github.com/abseil/abseil-cpp
cd abseil-cpp
git checkout 14550beb3b7b97195e483fb74b5efb906395c31e -b Jul302019 # Jul 30 2019
emcmake cmake -DCMAKE_CXX_STANDARD=17 "."
emmake make
cd

# WAVM (optional)
apt-get install -y --no-install-recommends llvm-6.0-dev
git clone https://github.com/WAVM/WAVM
cd WAVM
git checkout 1ec06cd202a922015c9041c5ed84f875453c4dc7 -b Oct152019 # Oct 15 2019
cmake "."
make
make install
cd
rm -rf WAVM
