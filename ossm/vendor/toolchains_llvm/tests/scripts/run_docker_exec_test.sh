#!/bin/bash
# Copyright 2024 The Bazel Authors.
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

os="$(uname -s)"
if [[ ${os} != "Darwin" ]]; then
  echo >&2 "Test, to be most effective, is meant to be run on Darwin."
  exit 1
fi

if "${CI:-false}"; then
  # macOS GitHub Action Runners do not have docker installed on them.
  echo >&2 "Test can not be run on GitHub Actions"
  exit 1
fi

scripts_dir="$(dirname "${BASH_SOURCE[0]}")"
source "${scripts_dir}/bazel.sh"
"${bazel}" version

cd "${scripts_dir}"

base_image="debian:stable-slim"
binpath="$("${bazel}" info "${common_args[@]}" bazel-bin)/stdlib_test"

docker build --platform=linux/amd64 --pull --tag=bazel-docker-sandbox - <<-EOF
	FROM ${base_image}
	ENV DEBIAN_FRONTEND=noninteractive
	RUN apt-get -qq update && \
		apt-get -qq -y install libtinfo5 libxml2 zlib1g-dev libxml2
EOF

build_args=(
  "${common_args[@]}"
  # Platforms
  "--platforms=@toolchains_llvm//platforms:linux-x86_64"
  "--extra_execution_platforms=@toolchains_llvm//platforms:linux-x86_64"
  "--extra_toolchains=@llvm_toolchain_linux_exec//:cc-toolchain-x86_64-linux"
  # Docker sandbox
  "--experimental_enable_docker_sandbox"
  "--experimental_docker_verbose"
  "--experimental_docker_image=bazel-docker-sandbox"
  "--spawn_strategy=docker"
  # Verbosity of build actions
  "--copt=-v"
  "--linkopt=-v"
  "--linkopt=-Wl,-v"
)

"${bazel}" --bazelrc=/dev/null build "${build_args[@]}" //:stdlib_test
file "${binpath}" | tee /dev/stderr | grep -q ELF
docker run --rm -it --platform=linux/amd64 \
  --mount "type=bind,source=${binpath},target=/stdlib_test" "${base_image}" /stdlib_test
