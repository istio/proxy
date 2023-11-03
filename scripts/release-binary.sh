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
set -ex

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

# Verify that we're building binaries on Ubuntu 18.04 (Bionic).
CHECK=1

# Defines the base binary name for artifacts. For example, this will be "envoy-debug".
BASE_BINARY_NAME="${BASE_BINARY_NAME:-"envoy"}"

# If enabled, we will just build the Envoy binary rather than wasm, etc
BUILD_ENVOY_BINARY_ONLY="${BUILD_ENVOY_BINARY_ONLY:-0}"

function usage() {
  echo "$0
    -d  The bucket name to store proxy binary (optional).
        If not provided, both envoy binary push and docker image push are skipped.
    -i  Skip Ubuntu Bionic check. DO NOT USE THIS FOR RELEASED BINARIES."
  exit 1
}

while getopts d:i arg ; do
  case "${arg}" in
    d) DST="${OPTARG}";;
    i) CHECK=0;;
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

# Make sure the release binaries are built on x86_64 Ubuntu 18.04 (Bionic)
if [ "${CHECK}" -eq 1 ] ; then
  if [[ "${BAZEL_BUILD_ARGS}" != *"--config=remote-"* ]]; then
    UBUNTU_RELEASE=${UBUNTU_RELEASE:-$(lsb_release -c -s)}
    [[ "${UBUNTU_RELEASE}" == 'bionic' ]] || { echo 'Must run on Ubuntu Bionic.'; exit 1; }
  fi
  [[ "$(uname -m)" == 'x86_64' ]] || { echo 'Must run on x86_64.'; exit 1; }
fi

# The proxy binary name.
SHA="$(git rev-parse --verify HEAD)"

if [ -n "${DST}" ]; then
  # If binary already exists skip.
  # Use the name of the last artifact to make sure that everything was uploaded.
  BINARY_NAME="${HOME}/istio-proxy-debug-${SHA}.deb"
  gsutil stat "${DST}/${BINARY_NAME}" \
    && { echo 'Binary already exists'; exit 0; } \
    || echo 'Building a new binary.'
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
      CONFIG_PARAMS="--config=debug"
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

  if [ -n "${DST}" ]; then
    # Copy it to the bucket.
    echo "Copying ${BINARY_NAME} ${SHA256_NAME} to ${DST}/"
    gsutil cp "${BINARY_NAME}" "${SHA256_NAME}" "${DWP_NAME}" "${DST}/"
  fi
done

echo "Checking extensions build config"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE="${ROOT}/WORKSPACE"

ENVOY_ORG="$(grep -Pom1 "^ENVOY_ORG = \"\K[a-zA-Z-]+" "${WORKSPACE}")"
ENVOY_REPO="$(grep -Pom1 "^ENVOY_REPO = \"\K[a-zA-Z-]+" "${WORKSPACE}")"
ENVOY_SHA="$(grep -Pom1 "^ENVOY_SHA = \"\K[a-zA-Z0-9]{40}" "${WORKSPACE}")"

TMP_DIR=$(mktemp -d)
ENVOY_EXTENSIONS_BUILD_CONFIG="${TMP_DIR}/envoy.bzl"
PROXY_EXTENSIONS_BUILD_CONFIG="${TMP_DIR}/proxy.bzl"

echo "get envoy extensions build config from ${ENVOY_ORG}/${ENVOY_REPO} commit: ${ENVOY_SHA}"
curl --silent --show-error --retry 10 --location \
      "https://raw.githubusercontent.com/${ENVOY_ORG}/${ENVOY_REPO}/${ENVOY_SHA}/source/extensions/extensions_build_config.bzl" \
      -o "${ENVOY_EXTENSIONS_BUILD_CONFIG}" \
    || { echo "Could not get envoy extensions build config." ; exit 1 ; }

# backup proxy extension build config
cp "${ROOT}/bazel/extension_config/extensions_build_config.bzl" "${TMP_DIR}/proxy.bzl"
# remove the first line
sed -i "1d" "${PROXY_EXTENSIONS_BUILD_CONFIG}"

go run tools/extension-check/main.go \
  --ignore-extensions tools/extension-check/wellknown-extensions \
  --envoy-extensions-build-config "${ENVOY_EXTENSIONS_BUILD_CONFIG}" \
  --proxy-extensions-build-config "${PROXY_EXTENSIONS_BUILD_CONFIG}" \
  || { echo "failed to check extension build config"; exit 1;}

# Exit early to skip wasm build
if [ "${BUILD_ENVOY_BINARY_ONLY}" -eq 1 ]; then
  exit 0
fi
