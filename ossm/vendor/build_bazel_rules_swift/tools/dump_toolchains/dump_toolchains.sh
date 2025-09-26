#!/bin/bash
#
# Copyright 2019 The Bazel Authors. All rights reserved.
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

set -euo pipefail

if [[ "$(uname)" != Darwin ]]; then
  echo "error: dumping toolchains is only supported on macOS"
  exit 1
fi

readonly toolchain_directories=(
  /Library/Developer/Toolchains
  ~/Library/Developer/Toolchains
)

shopt -s nullglob
for toolchain_directory in "${toolchain_directories[@]}"
do
  for toolchain in "$toolchain_directory"/*.xctoolchain
  do
    plist_path="$toolchain/Info.plist"

    if [[ ! -f "$plist_path" ]]; then
      echo "error: '$toolchain' is missing Info.plist"
      exit 1
    fi

    bundle_id=$(/usr/libexec/PlistBuddy -c "print :CFBundleIdentifier" "$plist_path")
    echo "$toolchain -> $bundle_id"
  done
done
