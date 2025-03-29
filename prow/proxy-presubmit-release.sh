#!/bin/bash
#
# Copyright 2017 Istio Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

WD=$(dirname "$0")
WD=$(cd "$WD" || exit 1 ; pwd)

#######################################
# Presubmit script triggered by Prow. #
#######################################
# shellcheck disable=SC1090,SC1091
source "${WD}/proxy-common.inc"

echo 'Test building release artifacts'

echo 'Test with llvm 18.1.8'

LLVM_VERSION=18.1.8
LLVM_BASE_URL=https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVM_VERSION}
LLVM_DIRECTORY=/usr/lib/llvm

case $(uname -m) in
    x86_64)
        LLVM_ARCHIVE=clang+llvm-${LLVM_VERSION}-x86_64-linux-gnu-ubuntu-18.04
        LLVM_ARTIFACT=clang+llvm-${LLVM_VERSION}-x86_64-linux-gnu-ubuntu-18.04
        ;;
    aarch64)
        LLVM_ARCHIVE=clang+llvm-${LLVM_VERSION}-aarch64-linux-gnu
        LLVM_ARTIFACT=clang+llvm-${LLVM_VERSION}-aarch64-linux-gnu
        ;;
    *) 
        echo "unsupported architecture"; 
        exit 1 
        ;;
esac

wget -nv ${LLVM_BASE_URL}/${LLVM_ARTIFACT}.tar.xz
tar -xJf ${LLVM_ARTIFACT}.tar.xz -C /tmp
mkdir -p ${LLVM_DIRECTORY}
rm -rf "${LLVM_DIRECTORY:?}/"
mv /tmp/${LLVM_ARCHIVE}/* ${LLVM_DIRECTORY}/

make test_release
