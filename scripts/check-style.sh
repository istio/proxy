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

CLANG_VERSION_REQUIRED="9.0.0"
CLANG_FORMAT=$(which clang-format)
CLANG_VERSION="$(${CLANG_FORMAT} -version 2>/dev/null | cut -d ' ' -f 3 | cut -d '-' -f 1)"
if [[ ! -x "${CLANG_FORMAT}" || "${CLANG_VERSION}" != "${CLANG_VERSION_REQUIRED}" ]]; then
  # Install required clang version to a folder and cache it.
  CLANG_DIRECTORY="${HOME}/clang"
  CLANG_FORMAT="${CLANG_DIRECTORY}/bin/clang-format"

  if [ "$(uname)" == "Darwin" ]; then
    CLANG_BIN="x86_64-darwin-apple.tar.xz"
  elif [ "$(expr substr $(uname -s) 1 5)" == "Linux" ]; then
    CLANG_BIN="x86_64-linux-gnu-ubuntu-14.04.tar.xz"
  else
    echo "Unsupported environment." ; exit 1 ;
  fi

  echo "Downloading clang-format: https://releases.llvm.org/${CLANG_VERSION_REQUIRED}/clang+llvm-${CLANG_VERSION_REQUIRED}-${CLANG_BIN}"
  echo "Installing required clang-format ${CLANG_VERSION_REQUIRED} to ${CLANG_DIRECTORY}"

  mkdir -p ${CLANG_DIRECTORY}
  curl --silent --show-error --retry 10 \
    "https://releases.llvm.org/${CLANG_VERSION_REQUIRED}/clang+llvm-${CLANG_VERSION_REQUIRED}-${CLANG_BIN}" \
    | tar Jx -C "${CLANG_DIRECTORY}" --strip=1 \
  || { echo "Could not install required clang-format. Skip formatting." ; exit 1 ; }
fi

BUILDIFIER=$(which buildifier)
if [[ ! -x "${BUILDIFIER}" ]]; then
  BUILDIFIER="${HOME}/bin/buildifier"
  if [[ ! -x "${BUILDIFIER}" ]]; then

    if [ "$(uname)" == "Darwin" ]; then
      BUILDIFIER_BIN="buildifier.osx"
    elif [ "$(expr substr $(uname -s) 1 5)" == "Linux" ]; then
      BUILDIFIER_BIN="buildifier"
    else
      echo "Unsupported environment." ; exit 1 ;
    fi

    echo "Downloading buildifier: https://github.com/bazelbuild/buildtools/releases/download/0.29.0/${BUILDIFIER_BIN}"

    mkdir -p "${HOME}/bin"
    curl --silent --show-error --retry 10 --location \
      "https://github.com/bazelbuild/buildtools/releases/download/0.29.0/${BUILDIFIER_BIN}" \
      -o "${BUILDIFIER}" \
    || { echo "Could not install required buildifier. Skip formatting." ; exit 1 ; }
    chmod +x ${BUILDIFIER}
  fi
fi

echo "Checking file format ..."

pushd ${ROOT} > /dev/null

SOURCE_FILES=($(git ls-tree -r HEAD --name-only | grep -E '\.(h|c|cc|proto)$'))
"${CLANG_FORMAT}" -style=Google -i "${SOURCE_FILES[@]}" \
  || { echo "Could not run clang-format." ; exit 1 ; }

CHANGED_SOURCE_FILES=($(git diff HEAD --name-only | grep -E '\.(h|c|cc|proto)$'))

BAZEL_FILES=($(git ls-tree -r HEAD --name-only | grep -E '(\.bzl|BUILD|WORKSPACE)$'))
"${BUILDIFIER}" "${BAZEL_FILES[@]}" \
  || { echo "Could not run buildifier." ; exit 1 ; }

CHANGED_BAZEL_FILES=($(git diff HEAD --name-only | grep -E '(\.bzl|BUILD|WORKSPACE)$'))

if [[ "${#CHANGED_SOURCE_FILES}" -ne 0 ]]; then
  echo -e "Source file(s) not formatted:\n${CHANGED_SOURCE_FILES[@]}"
fi
if [[ "${#CHANGED_BAZEL_FILES}" -ne 0 ]]; then
  echo -e "Bazel file(s) not formatted:\n${CHANGED_BAZEL_FILES[@]}"
fi
if [[ "${#CHANGED_SOURCE_FILES}" -ne 0 || "${#CHANGED_BAZEL_FILES}" -ne 0 ]]; then
  exit 1
fi
echo "All files are properly formatted."

popd
