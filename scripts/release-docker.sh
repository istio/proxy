#!/bin/bash
#
# Copyright 2016 Istio Authors. All Rights Reserved.
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
#
################################################################################
#
# Requires:
# - linux environment
# - docker authorization
#
set -e
set -x

# Use clang for the release builds.
export PATH=/usr/lib/llvm-9/bin:$PATH
export CC=${CC:-clang}
export CXX=${CXX:-clang++}

# Docker tag
export GIT_SHA="$(git rev-parse --verify HEAD)"
export TAG="${TAG:-${GIT_SHA}}"

# Add --config=libc++ if wasn't passed already.
if [[ "${BAZEL_BUILD_ARGS}" != *"--config=libc++"* ]]; then
  BAZEL_BUILD_ARGS="${BAZEL_BUILD_ARGS} --config=libc++"
fi

for config in release release-test debug
do
  case $config in
    "release" )
      PARAMS="--config=release";;
    "release-test")
      PARAMS="--config=clang-asan --config=release-symbol";;
    "debug")
      PARAMS="--config=debug";;
  esac
  bazel run ${BAZEL_BUILD_ARGS} ${PARAMS} //tools/docker:push_envoy
  bazel run ${BAZEL_BUILD_ARGS} ${PARAMS} //tools/docker:push_envoy_bionic
done
