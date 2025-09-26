# Copyright 2023 The Bazel Authors. All rights reserved.
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

bin_link_layer_1=$TEST_TMPDIR/link1
ln -s "$bin" "$bin_link_layer_1"
bin_link_layer_2=$TEST_TMPDIR/link2
ln -s "$bin_link_layer_1" "$bin_link_layer_2"

result=$(RUNFILES_DIR='' RUNFILES_MANIFEST_FILE='' $bin)
result_link_layer_1=$(RUNFILES_DIR='' RUNFILES_MANIFEST_FILE='' $bin_link_layer_1)
result_link_layer_2=$(RUNFILES_DIR='' RUNFILES_MANIFEST_FILE='' $bin_link_layer_2)

if [[ "$result" != "$result_link_layer_1" ]]; then
    echo "Output from test does not match output when invoked via a link;"
    echo "Output from test:"
    echo "$result"
    echo "Output when invoked via a link:"
    echo "$result_link_layer_1"
    exit 1
fi
if [[ "$result" != "$result_link_layer_2" ]]; then
    echo "Output from test does not match output when invoked via a link to a link;"
    echo "Output from test:"
    echo "$result"
    echo "Output when invoked via a link to a link:"
    echo "$result_link_layer_2"
    exit 1
fi

exit 0
