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
apt-get autoremove -y
apt-get clean
apt-get install -y --no-install-recommends ca-certificates git autoconf autotools-dev automake libtool cmake python-is-python3 zlib1g-dev make xz-utils libzstd-dev pkg-config

# The specific version of GCC does not actually matter as long as it's confirmed to work.
# That's why we explicitly pin gcc version (in this case to gcc 13) - it's the version
# which was tested to work.
apt-get install -y --no-install-recommends gcc-13 g++-13 cpp-13
export CC=gcc-13
export CXX=g++-13
export CPP=cpp-13

# get $HOME
cd

# install emscripten
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
git checkout 3.1.67
./emsdk install --shallow 3.1.67
./emsdk activate 3.1.67
cd ..

# set up build env
source emsdk/emsdk_env.sh
CXXFLAGS="--std=c++17 -O3 -flto -DSTANDALONE_WASM"
NUM_CPUS=$(nproc)
JOBS=$((NUM_CPUS>1 ? NUM_CPUS-1 : NUM_CPUS))

# protobuf (optional, includes abseil)
git clone https://github.com/protocolbuffers/protobuf
cd protobuf
git checkout v26.1
git submodule update --init --recursive
emcmake cmake -DCMAKE_CXX_STANDARD=17 -DCMAKE_CXX_FLAGS="$CXXFLAGS" -Dprotobuf_BUILD_TESTS=OFF "."
emmake make -j $JOBS
emmake make install
cd ..

# abseil (optional, and already included in protobuf)
#git clone https://github.com/abseil/abseil-cpp
#cd abseil-cpp
#git checkout 20240722.0
#emcmake cmake -DCMAKE_CXX_STANDARD=17 -DCMAKE_CXX_FLAGS="$CXXFLAGS" "."
#emmake make -j $JOBS
#emmake make install
#cd ..

# re2 (optional, depends on installed absl)
git clone https://github.com/google/re2
cd re2
git checkout 2023-07-01
emcmake cmake -DCMAKE_CXX_STANDARD=17 -DCMAKE_CXX_FLAGS="$CXXFLAGS" "."
emmake make -j $JOBS
emmake make install
cd ..
