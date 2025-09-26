#!/bin/bash
# this is the integration test checking various combinations of conflicting nodejs toolchain definitions

set -eu

for test_attr in test_*; do
  pushd $test_attr > /dev/null
  attr=${test_attr#test_}
  echo -n "testing conflict on $attr... "
  if bazel mod tidy &> error.txt; then
    echo "ERROR: bazel mod tidy should have failed with following MODULE.bazel:"
    cat MODULE.bazel
    exit 1
  elif ! grep "conflicting toolchains" error.txt > /dev/null; then
    echo "ERROR: expected bazel mod tidy to mention conflicting toolchains, found:"
    cat error.txt
    exit 1
  else
    echo "PASS"
  fi
  popd > /dev/null
done
