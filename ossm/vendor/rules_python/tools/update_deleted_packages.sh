#!/usr/bin/env bash
# Copyright 2023 The Bazel Authors. All rights reserved.
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

# For integration tests, we want to be able to glob() up the sources inside a nested package
# See explanation in .bazelrc
#
# This script ensures that we only delete subtrees that have something a file
# signifying a new bazel workspace, whether it be bzlmod or classic. Generic
# algorithm:
#   1. Get all directories where a WORKSPACE or MODULE.bazel exists.
#   2. For each of the directories, get all directories that contains a BUILD.bazel file.
#   3. Sort and remove duplicates.

set -euo pipefail

DIR="$(dirname $0)/.."
cd $DIR

# The sed -i.bak pattern is compatible between macos and linux
{
    echo "# Generated via './tools/update_deleted_packages.sh'"
    find examples tests gazelle \( -name WORKSPACE -or -name MODULE.bazel \) |
        xargs -n 1 dirname |
        xargs -I{} find {} \( -name BUILD -or -name BUILD.bazel \) |
        xargs -n 1 dirname |
        grep -v "gazelle/docs" |
        sort -u |
        sed 's/^/common --deleted_packages=/g' 
} | tee "$DIR"/.bazelrc.deleted_packages
