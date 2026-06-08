#!/bin/bash
# Copyright 2020 The Bazel Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -euo pipefail

images=(
  "ubuntu:20.04"
)

LLVM_VERSION="first:>=15.0.0,<17"

git_root=$(git rev-parse --show-toplevel)
readonly git_root

for image in "${images[@]}"; do
  docker pull "${image}"
  docker run --rm --entrypoint=/bin/bash --env USE_BZLMOD --volume="${git_root}:/src:ro" "${image}" -c """
set -exuo pipefail

# Common setup
export DEBIAN_FRONTEND=noninteractive
apt-get -qq update
apt-get -qq -y install apt-utils curl libtinfo5 libxml2 pkg-config zlib1g-dev >/dev/null
# The above command gives some verbose output that can not be silenced easily.
# https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=288778

# The WebAssembly tests use an LLVM version that is too new for the GNU libc
# distributed in Ubuntu 20.04.
disable_wasm_tests='-W'

# Run tests
cd /src
tests/scripts/run_tests.sh -v '${LLVM_VERSION}' \${disable_wasm_tests}
"""
done
