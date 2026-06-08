#!/bin/bash

# Runs Bazel build commands over rustfmt rules, where some are expected
# to fail.
#
# Can be run from anywhere within the rules_rust workspace.

set -euo pipefail

if [[ -z "${BUILD_WORKSPACE_DIRECTORY:-}" ]]; then
  echo "This script should be run under Bazel"
  exit 1
fi

cd "${BUILD_WORKSPACE_DIRECTORY}"

# Executes a bazel build command and handles the return value, exiting
# upon seeing an error.
#
# Takes two arguments:
# ${1}: The expected return code.
# ${2}: The target within "//test/rustfmt" to be tested.
function check_build_result() {
  local ret=0
  echo -n "Testing ${2}... "
  (bazel test //test/rustfmt:"${2}") || ret="$?" && true
  if [[ "${ret}" -ne "${1}" ]]; then
    >&2 echo "FAIL: Unexpected return code [saw: ${ret}, want: ${1}] building target //test/rustfmt:${2}"
    >&2 echo "  Run \"bazel test //test/rustfmt:${2}\" to see the output"
    exit 1
  else
    echo "OK"
  fi
}

function test_all_and_apply() {
  local -r TEST_OK=0
  local -r TEST_FAILED=3
  local -r VARIANTS=(rust_binary rust_library rust_shared_library rust_static_library)

  temp_dir="$(mktemp -d -t ci-XXXXXXXXXX)"
  new_workspace="${temp_dir}/rules_rust_test_rustfmt"

  mkdir -p "${new_workspace}/test/rustfmt" && \
  cp -r test/rustfmt/* "${new_workspace}/test/rustfmt/" && \
  cat << EOF > "${new_workspace}/MODULE.bazel"
module(name = "rules_rust_test_rustfmt")
bazel_dep(name = "rules_rust", version = "0.0.0")
local_path_override(
    module_name = "rules_rust",
    path = "${BUILD_WORKSPACE_DIRECTORY}",
)

bazel_dep(
    name = "bazel_skylib",
    version = "1.7.1",
)
bazel_dep(
    name = "rules_shell",
    version = "0.3.0",
)

rust = use_extension("@rules_rust//rust:extensions.bzl", "rust")
use_repo(rust, "rust_toolchains")
register_toolchains("@rust_toolchains//:all")
EOF
  # See github.com/bazelbuild/rules_rust/issues/2317.
  echo "build --noincompatible_sandbox_hermetic_tmp" > "${new_workspace}/.bazelrc"

  if [[ -f "${BUILD_WORKSPACE_DIRECTORY}/.bazelversion" ]]; then
    cp "${BUILD_WORKSPACE_DIRECTORY}/.bazelversion" "${new_workspace}/.bazelversion"
  fi

  # Drop the 'norustfmt' tags
  if [ "$(uname)" == "Darwin" ]; then
    SEDOPTS=(-i '' -e)
  else
    SEDOPTS=(-i)
  fi
  sed ${SEDOPTS[@]} 's/"norustfmt"//' "${new_workspace}/test/rustfmt/rustfmt_integration_test_suite.bzl"
  sed ${SEDOPTS[@]} 's/"manual"//' "${new_workspace}/test/rustfmt/rustfmt_integration_test_suite.bzl"

  pushd "${new_workspace}"

  for variant in ${VARIANTS[@]}; do
    check_build_result $TEST_FAILED ${variant}_unformatted_2015_test
    check_build_result $TEST_FAILED ${variant}_unformatted_2018_test
    check_build_result $TEST_OK ${variant}_formatted_2015_test
    check_build_result $TEST_OK ${variant}_formatted_2018_test
    check_build_result $TEST_OK ${variant}_generated_test
  done

  # Format a specific target
  for variant in ${VARIANTS[@]}; do
    bazel run @rules_rust//tools/rustfmt -- //test/rustfmt:${variant}_unformatted_2018
  done

  for variant in ${VARIANTS[@]}; do
    check_build_result $TEST_FAILED ${variant}_unformatted_2015_test
    check_build_result $TEST_OK ${variant}_unformatted_2018_test
    check_build_result $TEST_OK ${variant}_formatted_2015_test
    check_build_result $TEST_OK ${variant}_formatted_2018_test
    check_build_result $TEST_OK ${variant}_generated_test
  done

  # Format all targets
  bazel run @rules_rust//tools/rustfmt --@rules_rust//rust/settings:rustfmt.toml=//test/rustfmt:test_rustfmt.toml

  # Ensure all tests pass
  check_build_result $TEST_OK "*"

  popd

  rm -rf "${temp_dir}"
}

test_all_and_apply
