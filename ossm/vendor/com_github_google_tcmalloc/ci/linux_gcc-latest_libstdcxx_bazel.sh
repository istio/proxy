#!/bin/bash
#
# Copyright 2019 The TCMalloc Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This script that can be invoked to test tcmalloc in a hermetic environment
# using a Docker image on Linux. You must have Docker installed to use this
# script.

set -euox pipefail

if [ -z ${TCMALLOC_ROOT:-} ]; then
  TCMALLOC_ROOT="$(realpath $(dirname ${0})/..)"
fi

if [ -z ${STD:-} ]; then
  STD="c++17"
fi

if [ -z ${COMPILATION_MODE:-} ]; then
  COMPILATION_MODE="fastbuild opt"
fi

if [ -z ${EXCEPTIONS_MODE:-} ]; then
  EXCEPTIONS_MODE="-fno-exceptions -fexceptions"
fi

readonly DOCKER_CONTAINER="gcr.io/google.com/absl-177019/linux_hybrid-latest:20240523"

# USE_BAZEL_CACHE=1 only works on Kokoro.
# Without access to the credentials this won't work.
if [[ ${USE_BAZEL_CACHE:-0} -ne 0 ]]; then
  DOCKER_EXTRA_ARGS="--volume=${KOKORO_KEYSTORE_DIR}:/keystore:ro ${DOCKER_EXTRA_ARGS:-}"
  # Bazel doesn't track changes to tools outside of the workspace
  # (e.g. /usr/bin/gcc), so by appending the docker container to the
  # remote_http_cache url, we make changes to the container part of
  # the cache key. Hashing the key is to make it shorter and url-safe.
  container_key=$(echo ${DOCKER_CONTAINER} | sha256sum | head -c 16)
  BAZEL_EXTRA_ARGS="--remote_cache=https://storage.googleapis.com/absl-bazel-remote-cache/${container_key} --google_credentials=/keystore/73103_absl-bazel-remote-cache ${BAZEL_EXTRA_ARGS:-}"
fi

for std in ${STD}; do
  for compilation_mode in ${COMPILATION_MODE}; do
    for exceptions_mode in ${EXCEPTIONS_MODE}; do
      echo "--------------------------------------------------------------------"
      time docker run \
        --volume="${TCMALLOC_ROOT}:/tcmalloc:ro" \
        --workdir=/tcmalloc \
        --cap-add=SYS_PTRACE \
        --rm \
        -e CC="/usr/local/bin/gcc" \
        -e BAZEL_CXXOPTS="-std=${std}" \
        ${DOCKER_EXTRA_ARGS:-} \
        ${DOCKER_CONTAINER} \
        /usr/local/bin/bazel test ... \
          --compilation_mode="${compilation_mode}" \
          --copt="${exceptions_mode}" \
          --enable_bzlmod=false \
          --keep_going \
          --experimental_ui_max_stdouterr_bytes=-1 \
          --show_timestamps \
          --test_output=errors \
          --test_tag_filters=-benchmark \
          ${BAZEL_EXTRA_ARGS:-}
    done
  done
done
