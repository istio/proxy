#!/usr/bin/env bash
set -o errexit -o nounset -o pipefail

BZLMOD_FLAG="${BZLMOD_FLAG:-}"

function run_test {
  bazel run $BZLMOD_FLAG //:write_source_file_root-test
  local expected_out="test-out/dist/write_source_file_root-test/test.txt"
  if [ ! -e "$expected_out" ]; then
    echo "ERROR: expected $expected_out to exist"
    exit 1
  fi
  if [ -x "$expected_out" ]; then
    echo "ERROR: expected $expected_out to not be executable"
    exit 1
  fi
  expected_out="test-out/dist/write_source_file_root-test_b/test.txt"
  if [ ! -e "$expected_out" ]; then
    echo "ERROR: expected $expected_out to exist"
    exit 1
  fi
  if [ -x "$expected_out" ]; then
    echo "ERROR: expected $expected_out to not be executable"
    exit 1
  fi
}

# Run twice to make sure we can have permission to overwrite the outputs of a previous run
rm -Rf test-out
run_test
run_test
echo "All tests passed"
