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


function expect_match() {
  local expected_pattern=$1
  local actual=$2
  if ! (echo "$actual" | grep "$expected_pattern" ) >/dev/null; then
    echo "expected to match: $expected_pattern"
    echo "===== actual START ====="
    echo "$actual"
    echo "===== actual END ====="
    echo
    touch EXPECTATION_FAILED
    return 1
  fi
}


echo "Check inherited and disabled"
# Verify setting it to empty string disables safe path
actual=$(PYTHONSAFEPATH= $bin 2>&1)
expect_match "sys.flags.safe_path: False" "$actual"
expect_match "PYTHONSAFEPATH: EMPTY" "$actual"

echo "Check inherited and propagated"
# Verify setting it to any string enables safe path and that
# value is propagated
actual=$(PYTHONSAFEPATH=OUTER $bin 2>&1)
expect_match "sys.flags.safe_path: True" "$actual"
expect_match "PYTHONSAFEPATH: OUTER" "$actual"

echo "Check enabled by default"
# Verifying doing nothing leaves safepath enabled by default
actual=$($bin 2>&1)
expect_match "sys.flags.safe_path: True" "$actual"
expect_match "PYTHONSAFEPATH: 1" "$actual"

# Exit if any of the expects failed
[[ ! -e EXPECTATION_FAILED ]]
