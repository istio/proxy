#!/bin/bash
#
# Copyright 2016 Istio Authors. All Rights Reserved.
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
#
################################################################################
#
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

CLANG_FORMAT=$(command -v clang-format)
if [[ ! -x "${CLANG_FORMAT}" ]]; then
  echo "clang-format not found in PATH." ; exit 1 ;
fi

BUILDIFIER=$(command -v buildifier)
if [[ ! -x "${BUILDIFIER}" ]]; then
  BUILDIFIER="${HOME}/bin/buildifier"
  if [[ ! -x "${BUILDIFIER}" ]]; then

    if [ "$(uname)" == "Darwin" ]; then
      BUILDIFIER_BIN="buildifier-darwin-amd64"
    elif [[ "$(uname -s)" =~ Linux* ]]; then
      if [ "$(uname -m)" == "aarch64" ]; then
        BUILDIFIER_BIN="buildifier-linux-arm64"
      else
        BUILDIFIER_BIN="buildifier-linux-amd64"
      fi
    else
      echo "Unsupported environment." ; exit 1 ;
    fi

    echo "Downloading buildifier"

    mkdir -p "${HOME}/bin"
    curl --silent --show-error --retry 10 --location \
      "https://github.com/bazelbuild/buildtools/releases/download/v6.1.2/${BUILDIFIER_BIN}" \
      -o "${BUILDIFIER}" \
    || { echo "Could not install required buildifier. Skip formatting." ; exit 1 ; }
    chmod +x "${BUILDIFIER}"
  fi
fi

echo "Checking file format ..."

pushd "${ROOT}" > /dev/null || exit 1

SOURCE_FILES=()
while IFS='' read -r line; do SOURCE_FILES+=("$line"); done < <(git ls-tree -r HEAD --name-only | grep -E '\.(h|c|cc|proto)$')

"${CLANG_FORMAT}" -i "${SOURCE_FILES[@]}" \
  || { echo "Could not run clang-format." ; exit 1 ; }

CHANGED_SOURCE_FILES=()
while IFS='' read -r line; do CHANGED_SOURCE_FILES+=("$line"); done < <(git diff HEAD --name-only | grep -E '\.(h|c|cc|proto)$')

BAZEL_FILES=()
while IFS='' read -r line; do BAZEL_FILES+=("$line"); done < <(git ls-tree -r HEAD --name-only | grep -E '(\.bzl|BUILD|WORKSPACE)$' |grep -v 'extensions_build_config.bzl')

"${BUILDIFIER}" "${BAZEL_FILES[@]}" \
  || { echo "Could not run buildifier." ; exit 1 ; }

CHANGED_BAZEL_FILES=()
while IFS='' read -r line; do CHANGED_BAZEL_FILES+=("$line"); done < <(git diff HEAD --name-only | grep -E '(\.bzl|BUILD|WORKSPACE)$')

if [[ "${#CHANGED_SOURCE_FILES}" -ne 0 ]]; then
  echo -e "Source file(s) not formatted:\n${CHANGED_SOURCE_FILES[*]}"
  git diff HEAD -- "${CHANGED_SOURCE_FILES[@]}"
fi
if [[ "${#CHANGED_BAZEL_FILES}" -ne 0 ]]; then
  echo -e "Bazel file(s) not formatted:\n${CHANGED_BAZEL_FILES[*]}"
fi
if [[ "${#CHANGED_SOURCE_FILES}" -ne 0 || "${#CHANGED_BAZEL_FILES}" -ne 0 ]]; then
  exit 1
fi
echo "All files are properly formatted."

popd || exit 1
