#!/bin/bash
# Copyright 2025 The Bazel Authors. All rights reserved.
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

set -ueo pipefail

tag="${TAG:-alma9-bazel}"
volume="${VOLUME:-bazel-cache}"

dir=$(realpath "$(dirname "$0")")
repo_root="$dir/../.."

USER_ID=${USER_ID:-1000}
GROUP_ID=${GROUP_ID:-1000}
(
   set -x
   docker build -t "$tag" --build-arg USER_ID="$USER_ID" --build-arg GROUP_ID="$GROUP_ID" "$dir/docker"
)

docker_args=(
   -v "$volume":/home/devuser/.cache
   -v "$repo_root:$repo_root"
   -w "$PWD"
   --rm
   -i
   -t
)

set -x
exec docker run "${docker_args[@]}" "$tag" "$@"
