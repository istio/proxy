#!/bin/bash

# Runs Bazel build commands over clippy rules, where some are expected
# to fail.
#
# Can be run from anywhere within the rules_rust workspace.

set -euo pipefail

if [[ -z "${BUILD_WORKSPACE_DIRECTORY:-}" ]]; then
  >&2 echo "This script should be run under Bazel"
  exit 1
fi

cd "${BUILD_WORKSPACE_DIRECTORY}"

TEMP_DIR="$(mktemp -d -t ci-XXXXXXXXXX)"
NEW_WORKSPACE="${TEMP_DIR}/rules_rust_test_clippy"

# Executes a bazel build command and handles the return value, exiting
# upon seeing an error.
#
# Takes two arguments:
# ${1}: The expected return code.
# ${2}: The target within "//test/clippy" to be tested.
#
# Any additional arguments are passed to `bazel build`.
function check_build_result() {
  local ret=0
  echo -n "Testing ${2}... "
  (bazel build ${@:3} //test/clippy:"${2}" &> /dev/null) || ret="$?" && true
  if [[ "${ret}" -ne "${1}" ]]; then
    >&2 echo "FAIL: Unexpected return code [saw: ${ret}, want: ${1}] building target //test/clippy:${2}"
    >&2 echo "  Run \"bazel build //test/clippy:${2}\" to see the output"
    exit 1
  elif [[ $# -ge 3 ]] && [[ "${@:3}" == *"capture_clippy_output"* ]]; then
    # Make sure that content was written to the output file
    if [ "$(uname)" == "Darwin" ]; then
      STATOPTS=(-f%z)
    else
      STATOPTS=(-c%s)
    fi
    if [[ $(stat ${STATOPTS[@]} "${NEW_WORKSPACE}/bazel-bin/test/clippy/${2%_clippy}.clippy.out") == 0 ]]; then
      >&2 echo "FAIL: Output wasn't written to out file building target //test/clippy:${2}"
      >&2 echo "  Output file: ${NEW_WORKSPACE}/bazel-bin/test/clippy/${2%_clippy}.clippy.out"
      >&2 echo "  Run \"bazel build //test/clippy:${2}\" to see the output"
      exit 1
    else
      echo "OK"
    fi
  else
    echo "OK"
  fi
}

function test_all() {
  local -r BUILD_OK=0
  local -r BUILD_FAILED=1
  local -r CAPTURE_OUTPUT="--@rules_rust//rust/settings:capture_clippy_output=True --@rules_rust//rust/settings:error_format=json"
  local -r BAD_CLIPPY_TOML="--@rules_rust//rust/settings:clippy.toml=//too_many_args:clippy.toml"

  mkdir -p "${NEW_WORKSPACE}/test/clippy" && \
  cp -r test/clippy/* "${NEW_WORKSPACE}/test/clippy/" && \
  cat << EOF > "${NEW_WORKSPACE}/WORKSPACE.bazel"
workspace(name = "rules_rust_test_clippy")
local_repository(
    name = "rules_rust",
    path = "${BUILD_WORKSPACE_DIRECTORY}",
)
load("@rules_rust//rust:repositories.bzl", "rust_repositories")
rust_repositories()
EOF

  # Drop the 'noclippy' tags
  if [ "$(uname)" == "Darwin" ]; then
    SEDOPTS=(-i '' -e)
  else
    SEDOPTS=(-i)
  fi
  sed ${SEDOPTS[@]} 's/"noclippy"//' "${NEW_WORKSPACE}/test/clippy/BUILD.bazel"

  pushd "${NEW_WORKSPACE}"

  check_build_result $BUILD_OK ok_binary_clippy
  check_build_result $BUILD_OK ok_library_clippy
  check_build_result $BUILD_OK ok_shared_library_clippy
  check_build_result $BUILD_OK ok_static_library_clippy
  check_build_result $BUILD_OK ok_test_clippy
  check_build_result $BUILD_OK ok_proc_macro_clippy
  check_build_result $BUILD_FAILED bad_binary_clippy
  check_build_result $BUILD_FAILED bad_library_clippy
  check_build_result $BUILD_FAILED bad_shared_library_clippy
  check_build_result $BUILD_FAILED bad_static_library_clippy
  check_build_result $BUILD_FAILED bad_test_clippy
  check_build_result $BUILD_FAILED bad_proc_macro_clippy

  # When capturing output, clippy errors are treated as warnings and the build
  # should succeed.
  check_build_result $BUILD_OK bad_binary_clippy $CAPTURE_OUTPUT
  check_build_result $BUILD_OK bad_library_clippy $CAPTURE_OUTPUT
  check_build_result $BUILD_OK bad_shared_library_clippy $CAPTURE_OUTPUT
  check_build_result $BUILD_OK bad_static_library_clippy $CAPTURE_OUTPUT
  check_build_result $BUILD_OK bad_test_clippy $CAPTURE_OUTPUT
  check_build_result $BUILD_OK bad_proc_macro_clippy $CAPTURE_OUTPUT
  check_build_result $BUILD_OK ok_library_clippy $CAPTURE_OUTPUT --@rules_rust//rust/settings:clippy_flags=-Dclippy::pedantic

  # Test that we can make the ok_library_clippy fail when using an extra config file.
  # Proves that the config file is used and overrides default settings.
  check_build_result $BUILD_FAILED ok_library_clippy $BAD_CLIPPY_TOML
}

test_all
