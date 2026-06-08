#!/bin/bash
# Copyright 2018 The Bazel Authors.
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

toolchain_name=""
enable_omp_targets="1"
enable_wasm_tests="1"
LLVM_VERSION=""

while getopts "hOt:v:W" opt; do
  case "${opt}" in
  "h")
    echo "Usage:"
    echo "-t - Toolchain name to use for testing; default is llvm_toolchain"
    exit 2
    ;;
  "O")
    enable_omp_targets=""
    ;;
  "t")
    toolchain_name="${OPTARG}"
    ;;
  "v")
    LLVM_VERSION="${OPTARG}"
    ;;
  "W")
    enable_wasm_tests=""
    ;;
  *)
    echo "invalid option: -${OPTARG}"
    exit 1
    ;;
  esac
done

scripts_dir="$(dirname "${BASH_SOURCE[0]}")"
source "${scripts_dir}/bazel.sh"
"${bazel}" version

cd "${scripts_dir}"

set -x
test_args=(
  "--extra_toolchains=${toolchain_name}"
  "--copt=-v"
  "--linkopt=-Wl,-v"
  "--linkopt=-Wl,-t"
)

targets=(
  "//:all"
)
# :test_cxx_standard_is_20 builds with a version of the default toolchain, if
# we're trying to build with a different toolchain then it's likely the default
# toolchain won't work so :test_cxx_standard_is_20 won't build.
if [[ -z "${toolchain_name}" ]]; then
  targets+=("//:test_cxx_standard_is_20")
fi

if [[ -n "${enable_omp_targets}" ]]; then
  targets+=("//:omp_tests")
fi

if [[ -n "${LLVM_VERSION}" ]]; then
  echo "LLVM_VERSION=${LLVM_VERSION}"
  common_test_args+=(
    "--repo_env=LLVM_VERSION=${LLVM_VERSION}"
  )
fi

"${bazel}" ${TEST_MIGRATION:+"--strict"} --bazelrc=/dev/null test \
  "${common_test_args[@]}" "${test_args[@]}" "${targets[@]}"

# Note that the following flags are currently known to cause issues in migration tests:
# --incompatible_disallow_struct_provider_syntax # https://github.com/bazelbuild/bazel/issues/7347
# --incompatible_no_rule_outputs_param # from rules_rust

# WebAssembly tests use a separate (newer) version of LLVM to exercise support
# for experimental features such as wasm64, which can cause the CI environment
# to run out of disk space.
#
# Mitigate this by expunging the workspace before trying to build Wasm targets.
if [[ -z "${toolchain_name}" ]] && [[ -n "${enable_wasm_tests}" ]]; then
  # Redefine `test_args` without `--linkopt=-Wl,-v`, which breaks `wasm-ld`.
  #
  # https://github.com/llvm/llvm-project/issues/112836
  test_args=(
    "--copt=-v"
    "--linkopt=-Wl,-t"
  )
  wasm_targets=(
    "//wasm:all"
  )
  "${bazel}" clean --expunge
  # Remove the repo contents cache in addition to cleaning the work trees since
  # this is where the llvm toolchains are stored.
  user="$(id -un)"
  if [[ ${OSTYPE} == 'darwin'* ]]; then
    rm -rf "/private/var/tmp/_bazel_${user}/cache/repos/v1/contents"
  else
    rm -rf "${HOME}/.cache/bazel/_bazel_${user}/cache/repos/v1/contents"
  fi
  "${bazel}" ${TEST_MIGRATION:+"--strict"} --bazelrc=/dev/null test \
    "${common_test_args[@]}" "${test_args[@]}" "${wasm_targets[@]}"
fi
