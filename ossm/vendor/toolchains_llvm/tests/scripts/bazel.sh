# Copyright 2022 The Bazel Authors.
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

# shellcheck shell=bash

short_uname="$(uname -s)"
readonly short_uname

os="$(echo "${short_uname}" | tr "[:upper:]" "[:lower:]")"
readonly os

arch="$(uname -m)"
if [[ ${arch} == "x86_64" ]]; then
  arch="amd64"
elif [[ ${arch} == "aarch64" ]] || [[ ${arch} == "arm64" ]]; then
  arch="arm64"
else
  echo >&2 "Unknown architecture: ${arch}"
fi
readonly arch

# Use bazelisk to catch migration problems.
readonly bazelisk_version="v1.28.0"
readonly url="https://github.com/bazelbuild/bazelisk/releases/download/${bazelisk_version}/bazelisk-${os}-${arch}"
bazel="${TMPDIR:-/tmp}/bazelisk"
readonly bazel

common_args=(
  "--enable_bzlmod=${USE_BZLMOD:-true}"
  "--enable_workspace"
)

# shellcheck disable=SC2034
common_test_args=(
  "${common_args[@]}"
  "--symlink_prefix=/"
  "--color=yes"
  "--show_progress_rate_limit=30"
  "--keep_going"
  "--test_output=errors"
  "--features=layering_check"
  "--process_headers_in_dependencies"
)

# Do not run autoconf to configure local CC toolchains.
export BAZEL_DO_NOT_DETECT_CPP_TOOLCHAIN=1

curl -L -sSf -o "${bazel}" "${url}"
chmod a+x "${bazel}"
