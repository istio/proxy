#!/usr/bin/env bash
set -o errexit -o nounset -o pipefail

BZLMOD_FLAG="${BZLMOD_FLAG:-}"

function run_test {
  bazel run $BZLMOD_FLAG //lib/tests/write_source_files:write_dist
  local expected_out="lib/tests/write_source_files/dist.js"
  if [ ! -e "$expected_out" ]; then
    echo "ERROR: expected $expected_out to exist"
    exit 1
  fi
  if [ -x "$expected_out" ]; then
    echo "ERROR: expected $expected_out to not be executable"
    exit 1
  fi

  bazel run $BZLMOD_FLAG //lib/tests/write_source_files:write_dist_executable
  local expected_out="lib/tests/write_source_files/dist_executable.js"
  if [ ! -e "$expected_out" ]; then
    echo "ERROR: expected $expected_out to exist"
    exit 1
  fi
  if [ ! -x "$expected_out" ]; then
    echo "ERROR: expected $expected_out to be executable"
    exit 1
  fi
}

# Run twice to make sure we can have permission to overwrite the outputs of a previous run
rm -f lib/tests/write_source_files/dist.js
rm -f lib/tests/write_source_files/dist_executable.js
run_test
run_test
echo "All tests passed"
