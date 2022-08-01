#!/bin/bash
# Copyright 2017 Istio Authors. All Rights Reserved.
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

# Example usage:
#
# bin/push-debian.sh \
#   -c opt
#   -v 0.2.1
#   -p gs://istio-release/release/0.2.1/deb

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
BAZEL_TARGET='//tools/deb:istio-proxy'
BAZEL_BINARY="${ROOT}/bazel-bin/tools/deb/istio-proxy"
ISTIO_VERSION=''
GCS_PATH=""
OUTPUT_DIR=""

# Add --config=libc++ if wasn't passed already.
if [[ "$(uname)" != "Darwin" && "${BAZEL_BUILD_ARGS}" != *"--config=libc++"* ]]; then
  BAZEL_BUILD_ARGS="${BAZEL_BUILD_ARGS} --config=libc++"
fi

# ARCH_SUFFIX allows optionally appending a -{ARCH} suffix to published binaries.
# For backwards compatibility, Istio skips this for amd64.
# Note: user provides "arm64"; we expand to "-arm64" for simple usage in script.
export ARCH_SUFFIX="${ARCH_SUFFIX+-${ARCH_SUFFIX}}"

set -o errexit
set -o nounset
set -o pipefail
set -x

function usage() {
  echo "$0
    -o directory to copy files
    -p <GCS path, e.g. gs://istio-release/release/0.2.1/deb>
    -v <istio version number>"
  exit 1
}

while getopts ":o:p:v:" arg; do
  case ${arg} in
    o) OUTPUT_DIR="${OPTARG}";;
    p) GCS_PATH="${OPTARG}";;
    v) ISTIO_VERSION="${OPTARG}";;
    *) usage;;
  esac
done

if [[ -n "${ISTIO_VERSION}" ]]; then
  BAZEL_BUILD_ARGS+=" --action_env=ISTIO_VERSION"
  export ISTIO_VERSION
fi

[[ -z "${GCS_PATH}" ]] && [[ -z "${OUTPUT_DIR}" ]] && usage


ARCH_NAME="k8"
case "$(uname -m)" in
  aarch64) ARCH_NAME="aarch64";;
esac

# Symlinks don't work, use full path as a temporary workaround.
# See: https://github.com/istio/istio/issues/15714 for details.
# k8-opt is the output directory for x86_64 optimized builds (-c opt, so --config=release-symbol and --config=release).
# shellcheck disable=SC2086
BAZEL_OUT="$(bazel info ${BAZEL_BUILD_ARGS} output_path)/${ARCH_NAME}-opt/bin"
BAZEL_BINARY="${BAZEL_OUT}/tools/deb/istio-proxy"

# shellcheck disable=SC2086
bazel build ${BAZEL_BUILD_ARGS} --config=release ${BAZEL_TARGET}

if [[ -n "${GCS_PATH}" ]]; then
  gsutil -m cp -r "${BAZEL_BINARY}.deb" "${GCS_PATH}/istio-proxy${ARCH_SUFFIX}.deb"
fi

if [[ -n "${OUTPUT_DIR}" ]]; then
  mkdir -p "${OUTPUT_DIR}/"
  cp -f "${BAZEL_BINARY}.deb" "${OUTPUT_DIR}/istio-proxy${ARCH_SUFFIX}.deb"
fi
