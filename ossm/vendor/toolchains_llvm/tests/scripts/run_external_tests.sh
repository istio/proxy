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

scripts_dir="$(dirname "${BASH_SOURCE[0]}")"
source "${scripts_dir}/bazel.sh"
"${bazel}" version

cd "${scripts_dir}"

# Generate some files needed for the tests.
"${bazel}" query "${common_args[@]}" @io_bazel_rules_go//tests/core/cgo:dylib_test >/dev/null

output_base="$("${bazel}" info output_base)"
echo "Output base: ${output_base}"

# As of rules_go 0.51.0 the 'generate_imported_dylib.sh' expects 'cc' to be available through PATH.
if [[ ${USE_BZLMOD} == "true" ]]; then
  generate_imported_dylib_sh="${output_base}/external/rules_go~/tests/core/cgo/generate_imported_dylib.sh"
  if [[ ! -f "${generate_imported_dylib_sh}" ]]; then
    generate_imported_dylib_sh="${output_base}/external/rules_go+/tests/core/cgo/generate_imported_dylib.sh"
  fi
else
  generate_imported_dylib_sh="${output_base}/external/io_bazel_rules_go/tests/core/cgo/generate_imported_dylib.sh"
fi
"${generate_imported_dylib_sh}" || echo "ERROR: rules_go script 'tests/core/cgo/generate_imported_dylib.sh' failed."

test_args=(
  "${common_test_args[@]}"
  "--copt=-Wno-deprecated-builtins" # https://github.com/abseil/abseil-cpp/issues/1201
  # Disable the "hermetic sandbox /tmp" behavior of Bazel 7 as it results in broken symlinks when
  # rules_foreign_cc builds pcre.
  # TODO: Remove this once rules_foreign_cc is fully compatible with Bazel 7.
  "--sandbox_add_mount_pair=/tmp"
)

# We exclude the following targets:
# - cc_libs_test from rules_go because it assumes that stdlibc++ has been dynamically linked, but we
#   link it statically on linux.
# - external_includes_test from rules_go because it is a nested bazel test and so takes a long time
#   to run, and it is not particularly useful to us.
# - time_zone_format_test from abseil-cpp because it assumes TZ is set to America/Los_Angeles, but
#   we run the tests in UTC.
# - {cdylib,bin}_has_native_dep_and_alwayslink_test from rules_rust because they assume the test is
#    being run in the root module (use 'rules_rust' in the bazel-bin path instead of 'rules_rust~').
# shellcheck disable=SC2207
absl_targets=($("${bazel}" query "${common_args[@]}" 'attr(timeout, short, tests(@com_google_absl//absl/...) except attr(tags, benchmark, tests(@com_google_absl//absl/...)))'))
"${bazel}" --bazelrc=/dev/null test "${test_args[@]}" -- \
  //foreign:pcre \
  @boringssl//... \
  @rules_rust//test/unit/{interleaved_cc_info,native_deps}:all \
  @io_bazel_rules_go//tests/core/cgo:all \
  -@io_bazel_rules_go//tests/core/cgo:cc_libs_test \
  -@io_bazel_rules_go//tests/core/cgo:cgo_abs_paths_test \
  -@io_bazel_rules_go//tests/core/cgo:external_includes_test \
  -@io_bazel_rules_go//tests/core/cgo:wrapped_cgo_test \
  -@rules_rust//test/unit/native_deps:{cdylib,bin}_has_native_dep_and_alwayslink_test \
  "${absl_targets[@]}" \
  -@com_google_absl//absl/time/internal/cctz:time_zone_format_test
