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
set -e
set +x

# Use clang for the release builds.
export PATH=/usr/lib/llvm/bin:$PATH
export CC=${CC:-/usr/lib/llvm/bin/clang}
export CXX=${CXX:-/usr/lib/llvm/bin/clang++}

# ARCH_SUFFIX allows optionally appending a -{ARCH} suffix to published binaries.
# For backwards compatibility, Istio skips this for amd64.
# Note: user provides "arm64"; we expand to "-arm64" for simple usage in script.
export ARCH_SUFFIX="${ARCH_SUFFIX+-${ARCH_SUFFIX}}"

# Expliticly stamp.
BAZEL_BUILD_ARGS="${BAZEL_BUILD_ARGS} --stamp"

if [[ "$(uname)" == "Darwin" ]]; then
  BAZEL_CONFIG_ASAN="--config=macos-asan"
else
  BAZEL_CONFIG_ASAN="--config=clang-asan-ci"
fi

# The bucket name to store proxy binaries.
DST=""

# Defines the base binary name for artifacts. For example, this will be "envoy-debug".
BASE_BINARY_NAME="${BASE_BINARY_NAME:-"envoy"}"

# If enabled, we will just build the Envoy binary rather than wasm, etc
BUILD_ENVOY_BINARY_ONLY="${BUILD_ENVOY_BINARY_ONLY:-0}"

function usage() {
  echo "$0
    -d  The bucket name to store proxy binary (optional).
        If not provided, both envoy binary push and docker image push are skipped."
  exit 1
}

while getopts d: arg ; do
  case "${arg}" in
    d) DST="${OPTARG}";;
    *) usage;;
  esac
done

if [[ "${BUILD_ENVOY_BINARY_ONLY}" != 1 && "${ARCH_SUFFIX}" != "" ]]; then
  # This is not a fundamental limitation; however, the support for the other release types
  # has not been updated to support this.
  echo "ARCH_SUFFIX currently requires BUILD_ENVOY_BINARY_ONLY"
  exit 1
fi

echo "Destination bucket: $DST"

if [ "${DST}" == "none" ]; then
  DST=""
fi

# Expected glibc version from the hermetic sysroot configured in WORKSPACE.
EXPECTED_GLIBC=$(grep -oP 'glibc_version\s*=\s*"\K[^"]+' WORKSPACE)

# The proxy binary name.
SHA="$(git rev-parse --verify HEAD)"

if [ -n "${DST}" ]; then
  # If binary already exists skip.
  # Use the name of the last artifact to make sure that everything was uploaded.
  BINARY_NAME="${HOME}/istio-proxy-debug-${SHA}.deb"
  ENDPOINT="$(echo "${CF_CREDENTIALS}" | jq -r '.endpoint' | tr -d '\n')"
  AWS_ACCESS_KEY_ID="$(echo "${CF_CREDENTIALS}" | jq -r '.access_key' | tr -d '\n')"
  AWS_SECRET_ACCESS_KEY="$(echo "${CF_CREDENTIALS}" | jq -r '.secret_key' | tr -d '\n')"
  AWS_REGION="$(echo "${CF_CREDENTIALS}" | jq -r '.region' | tr -d '\n')"
  AWS_SESSION_TOKEN="$(echo "${CF_CREDENTIALS}" | jq -r '.session_token' | tr -d '\n')"
  export AWS_ACCESS_KEY_ID AWS_SECRET_ACCESS_KEY AWS_REGION AWS_SESSION_TOKEN

  R2_EXISTS=0
  aws s3 ls "${DST}/${BINARY_NAME}" --endpoint-url "${ENDPOINT}" && R2_EXISTS=1

  if [ "${R2_EXISTS}" -eq 1 ]; then
    echo 'Binary already exists'; exit 0
  else
    echo 'Building a new binary.'
  fi
fi

ARCH_NAME="k8"
case "$(uname -m)" in
  aarch64) ARCH_NAME="aarch64";;
esac

# BAZEL_OUT: Symlinks don't work, use full path as a temporary workaround.
# See: https://github.com/istio/istio/issues/15714 for details.
# k8-opt is the output directory for x86_64 optimized builds (-c opt, so --config=release-symbol and --config=release).
# k8-dbg is the output directory for -c dbg builds.
for config in release release-symbol asan debug
do
  case $config in
    "release" )
      CONFIG_PARAMS="--config=release"
      BINARY_BASE_NAME="${BASE_BINARY_NAME}-alpha"
      # shellcheck disable=SC2086
      BAZEL_OUT="$(bazel info ${BAZEL_BUILD_ARGS} output_path)/${ARCH_NAME}-opt/bin"
      ;;
    "release-symbol")
      CONFIG_PARAMS="--config=release-symbol"
      BINARY_BASE_NAME="${BASE_BINARY_NAME}-symbol"
      # shellcheck disable=SC2086
      BAZEL_OUT="$(bazel info ${BAZEL_BUILD_ARGS} output_path)/${ARCH_NAME}-opt/bin"
      ;;
    "asan")
      # Asan is skipped on ARM64
      if [[ "$(uname -m)" != "aarch64" ]]; then
        # NOTE: libc++ is dynamically linked in this build.
        CONFIG_PARAMS="${BAZEL_CONFIG_ASAN} --config=release-symbol"
        BINARY_BASE_NAME="${BASE_BINARY_NAME}-asan"
        # shellcheck disable=SC2086
        BAZEL_OUT="$(bazel info ${BAZEL_BUILD_ARGS} output_path)/${ARCH_NAME}-opt/bin"
      fi
      ;;
    "debug")
      CONFIG_PARAMS="-c dbg"
      BINARY_BASE_NAME="${BASE_BINARY_NAME}-debug"
      # shellcheck disable=SC2086
      BAZEL_OUT="$(bazel info ${BAZEL_BUILD_ARGS} output_path)/${ARCH_NAME}-dbg/bin"
      ;;
  esac

  export BUILD_CONFIG=${config}

  echo "Building ${config} proxy"
  BINARY_NAME="${HOME}/${BINARY_BASE_NAME}-${SHA}${ARCH_SUFFIX}.tar.gz"
  DWP_NAME="${HOME}/${BINARY_BASE_NAME}-${SHA}${ARCH_SUFFIX}.dwp"
  SHA256_NAME="${HOME}/${BINARY_BASE_NAME}-${SHA}${ARCH_SUFFIX}.sha256"
  # shellcheck disable=SC2086
  bazel build ${BAZEL_BUILD_ARGS} ${CONFIG_PARAMS} //:envoy_tar //:envoy.dwp
  BAZEL_TARGET="${BAZEL_OUT}/envoy_tar.tar.gz"
  DWP_TARGET="${BAZEL_OUT}/envoy.dwp"
  cp -f "${BAZEL_TARGET}" "${BINARY_NAME}"
  cp -f "${DWP_TARGET}" "${DWP_NAME}"
  sha256sum "${BINARY_NAME}" > "${SHA256_NAME}"

  # Verify the binary doesn't require a newer glibc than the hermetic sysroot provides.
  if [ -n "${EXPECTED_GLIBC}" ]; then
    VERIFY_TMPDIR=$(mktemp -d)
    tar xf "${BINARY_NAME}" -C "${VERIFY_TMPDIR}"
    ENVOY_BIN=$(find "${VERIFY_TMPDIR}" -name envoy -type f)
    MAX_GLIBC=$(readelf -V "${ENVOY_BIN}" 2>/dev/null \
      | grep -oP 'GLIBC_\K[0-9]+\.[0-9]+' \
      | sort -uV | tail -1)
    rm -rf "${VERIFY_TMPDIR}"
    if [ "$(printf '%s\n' "${EXPECTED_GLIBC}" "${MAX_GLIBC}" | sort -V | tail -1)" != "${EXPECTED_GLIBC}" ]; then
      echo "ERROR: ${config} binary requires GLIBC_${MAX_GLIBC}, but sysroot targets ${EXPECTED_GLIBC}"
      exit 1
    fi
    echo "OK: ${config} binary requires GLIBC_${MAX_GLIBC} (<= sysroot ${EXPECTED_GLIBC})"
  fi

  if [ -n "${DST}" ]; then
    # Copy it to the bucket.
    echo "Copying ${BINARY_NAME} ${SHA256_NAME} to ${DST}/"

    for f in "${BINARY_NAME}" "${SHA256_NAME}" "${DWP_NAME}"; do
      echo "Copying $(basename "${f}") to R2"
      aws s3 cp "${f}" "${DST}/$(basename "${f}")" --endpoint-url "${ENDPOINT}"
    done
  fi
done

# Exit early to skip wasm build
if [ "${BUILD_ENVOY_BINARY_ONLY}" -eq 1 ]; then
  exit 0
fi
