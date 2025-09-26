# Copyright 2024 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# --- begin runfiles.bash initialization v3 ---
# Copy-pasted from the Bazel Bash runfiles library v3.
set -uo pipefail; set +e; f=bazel_tools/tools/bash/runfiles/runfiles.bash
source "${RUNFILES_DIR:-/dev/null}/$f" 2>/dev/null || \
  source "$(grep -sm1 "^$f " "${RUNFILES_MANIFEST_FILE:-/dev/null}" | cut -f2- -d' ')" 2>/dev/null || \
  source "$0.runfiles/$f" 2>/dev/null || \
  source "$(grep -sm1 "^$f " "$0.runfiles_manifest" | cut -f2- -d' ')" 2>/dev/null || \
  source "$(grep -sm1 "^$f " "$0.exe.runfiles_manifest" | cut -f2- -d' ')" 2>/dev/null || \
  { echo>&2 "ERROR: cannot find $f"; exit 1; }; f=; set -e
# --- end runfiles.bash initialization v3 ---
set +e

bin=$(rlocation $BIN_RLOCATION)
if [[ -z "$bin" ]]; then
  echo "Unable to locate test binary: $BIN_RLOCATION"
  exit 1
fi

function test_invocation() {
  actual=$($bin)
  # How we detect if a zip file was executed from depends on which bootstrap
  # is used.
  # bootstrap_impl=script outputs RULES_PYTHON_ZIP_DIR=<somepath>
  # bootstrap_impl=system_python outputs file:.*Bazel.runfiles
  expected_pattern="Hello"
  if ! (echo "$actual" | grep "$expected_pattern" ) >/dev/null; then
    echo "Test case failed: $1"
    echo "expected output to match: $expected_pattern"
    echo "but got:\n$actual"
    exit 1
  fi
}

# Test invocation with RUNFILES_DIR set
unset RUNFILES_MANIFEST_FILE
if [[ ! -e "$RUNFILES_DIR" ]]; then
  echo "Runfiles doesn't exist: $RUNFILES_DIR"
  exit 1
fi
test_invocation "using RUNFILES_DIR"


orig_runfiles_dir="$RUNFILES_DIR"
unset RUNFILES_DIR

# Test invocation using manifest within runfiles directory (output manifest)
# NOTE: this file may not actually exist in our test, but that's OK; the
# bootstrap just uses the path to find the runfiles directory.
export RUNFILES_MANIFEST_FILE="$orig_runfiles_dir/MANIFEST"
test_invocation "using RUNFILES_MANIFEST_FILE with output manifest"

# Test invocation using manifest outside runfiles (input manifest)
# NOTE: this file may not actually exist in our test, but that's OK; the
# bootstrap just uses the path to find the runfiles directory.
export RUNFILES_MANIFEST_FILE="${orig_runfiles_dir%%.runfiles}.runfiles_manifest"
test_invocation "using RUNFILES_MANIFEST_FILE with input manifest"

# Test invocation without any runfiles env vars set
unset RUNFILES_MANIFEST_FILE
test_invocation "using no runfiles env vars"
