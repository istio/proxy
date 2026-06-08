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

while getopts "h" opt; do
  case "${opt}" in
  "h")
    echo "Usage: No options"
    exit 2
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

set -x
test_args=(
  "--check_direct_dependencies=off"
)

targets=(
  "//toolchain/..."
)

if [[ -z "${common_test_args:-}" ]]; then
  common_test_args=()
fi

"${bazel}" ${TEST_MIGRATION:+"--strict"} --bazelrc=/dev/null test \
  "${common_test_args[@]}" "${test_args[@]}" -- "${targets[@]}"
