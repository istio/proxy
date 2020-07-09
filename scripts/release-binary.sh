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
export PATH=/usr/lib/llvm-9/bin:$PATH
export CC=${CC:-clang}
export CXX=${CXX:-clang++}

# Add --config=libc++ if wasn't passed already.
if [[ "$(uname)" != "Darwin" && "${BAZEL_BUILD_ARGS}" != *"--config=libc++"* ]]; then
  BAZEL_BUILD_ARGS="${BAZEL_BUILD_ARGS} --config=libc++"
fi

if [[ "$(uname)" == "Darwin" ]]; then
  BAZEL_CONFIG_ASAN="--config=macos-asan"
else
  BAZEL_CONFIG_ASAN="--config=clang-asan"
fi

# The bucket name to store proxy binaries.
DST=""

# Verify that we're building binaries on Ubuntu 16.04 (Xenial).
CHECK=1

# Defines the base binary name for artifacts. For example, this will be "envoy-debug".
BINARY_NAME="${BINARY_NAME:-"envoy"}"

# If enabled, we will just build the Envoy binary rather then docker images, deb, wasm, etc
BUILD_ENVOY_BINARY_ONLY="${BUILD_ENVOY_BINARY_ONLY:-0}"

# Push envoy docker image.
PUSH_DOCKER_IMAGE=0

function usage() {
  echo "$0
    -d  The bucket name to store proxy binary (optional).
        If not provided, both envoy binary push and docker image push are skipped.
    -i  Skip Ubuntu Xenial check. DO NOT USE THIS FOR RELEASED BINARIES.
        Cannot be used together with -d option.
    -p  Push envoy docker image.
        Registry is hard coded to gcr.io and repository is controlled via DOCKER_REPOSITORY env var."
  exit 1
}

while getopts d:ipc arg ; do
  case "${arg}" in
    d) DST="${OPTARG}";;
    i) CHECK=0;;
    p) PUSH_DOCKER_IMAGE=1;;
    *) usage;;
  esac
done

echo "Destination bucket: $DST"

if [ "${DST}" == "none" ]; then
  DST=""
fi

# Make sure the release binaries are built on x86_64 Ubuntu 16.04 (Xenial)
if [ "${CHECK}" -eq 1 ]; then
  if [[ "${BAZEL_BUILD_ARGS}" != *"--config=remote-"* ]]; then
    UBUNTU_RELEASE=${UBUNTU_RELEASE:-$(lsb_release -c -s)}
    [[ "${UBUNTU_RELEASE}" == 'xenial' ]] || { echo 'Must run on Ubuntu 16.04 (Xenial).'; exit 1; }
  fi
  [[ "$(uname -m)" == 'x86_64' ]] || { echo 'Must run on x86_64.'; exit 1; }
elif [ -n "${DST}" ]; then
  echo "The -i option is not allowed together with -d option."
  exit 1
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

# BAZEL_OUT: Symlinks don't work, use full path as a temporary workaround.
# See: https://github.com/istio/istio/issues/15714 for details.
# k8-opt is the output directory for x86_64 optimized builds (-c opt, so --config=release-symbol and --config=release).
# k8-dbg is the output directory for -c dbg builds.
for config in release release-symbol debug
do
  case $config in
    "release" )
      CONFIG_PARAMS="--config=release"
      BINARY_BASE_NAME="envoy-alpha"
      PACKAGE_BASE_NAME="istio-proxy"
      # shellcheck disable=SC2086
      BAZEL_OUT="$(bazel info ${BAZEL_BUILD_ARGS} output_path)/k8-opt/bin"
      ;;
    "release-symbol")
      CONFIG_PARAMS="--config=release-symbol"
      BINARY_BASE_NAME="envoy-symbol"
      PACKAGE_BASE_NAME=""
      # shellcheck disable=SC2086
      BAZEL_OUT="$(bazel info ${BAZEL_BUILD_ARGS} output_path)/k8-opt/bin"
      ;;
    "asan")
      # NOTE: libc++ is dynamically linked in this build.
      PUSH_DOCKER_IMAGE=0
      CONFIG_PARAMS="${BAZEL_CONFIG_ASAN} --config=release-symbol"
      BINARY_BASE_NAME="envoy-asan"
      PACKAGE_BASE_NAME=""
      # shellcheck disable=SC2086
      BAZEL_OUT="$(bazel info ${BAZEL_BUILD_ARGS} output_path)/k8-opt/bin"
      ;;
    "debug")
      CONFIG_PARAMS="--config=debug"
      BINARY_BASE_NAME="envoy-debug"
      PACKAGE_BASE_NAME="istio-proxy-debug"
      # shellcheck disable=SC2086
      BAZEL_OUT="$(bazel info ${BAZEL_BUILD_ARGS} output_path)/k8-dbg/bin"
      ;;
  esac

  export BUILD_CONFIG=${config}

  echo "Building ${config} proxy"
  BINARY_NAME="${HOME}/${BINARY_BASE_NAME}-${SHA}.tar.gz"
  SHA256_NAME="${HOME}/${BINARY_BASE_NAME}-${SHA}.sha256"
  # shellcheck disable=SC2086
  bazel build ${BAZEL_BUILD_ARGS} ${CONFIG_PARAMS} //src/envoy:envoy_tar
  BAZEL_TARGET="${BAZEL_OUT}/src/envoy/envoy_tar.tar.gz"
  cp -f "${BAZEL_TARGET}" "${BINARY_NAME}"
  sha256sum "${BINARY_NAME}" > "${SHA256_NAME}"

  if [ -n "${DST}" ]; then
    # Copy it to the bucket.
    echo "Copying ${BINARY_NAME} ${SHA256_NAME} to ${DST}/"
    gsutil cp "${BINARY_NAME}" "${SHA256_NAME}" "${DST}/"
  fi

  if [ "${BUILD_ENVOY_BINARY_ONLY}" -eq 1 ]; then
    continue
  fi

  echo "Building ${config} docker image"
  # shellcheck disable=SC2086
  bazel build ${BAZEL_BUILD_ARGS} ${CONFIG_PARAMS} \
    //tools/docker:envoy_distroless \
    //tools/docker:envoy_ubuntu

  if [ "${PUSH_DOCKER_IMAGE}" -eq 1 ]; then
    echo "Pushing ${config} docker image"
    # shellcheck disable=SC2086
    bazel run ${BAZEL_BUILD_ARGS} ${CONFIG_PARAMS} \
      //tools/docker:push_envoy_distroless \
      //tools/docker:push_envoy_ubuntu
  fi

  if [ -n "${PACKAGE_BASE_NAME}" ]; then
    echo "Building ${config} debian package"
    BINARY_NAME="${HOME}/${PACKAGE_BASE_NAME}-${SHA}.deb"
    SHA256_NAME="${HOME}/${PACKAGE_BASE_NAME}-${SHA}.sha256"
    # shellcheck disable=SC2086
    bazel build ${BAZEL_BUILD_ARGS} ${CONFIG_PARAMS} //tools/deb:istio-proxy
    BAZEL_TARGET="${BAZEL_OUT}/tools/deb/istio-proxy.deb"
    cp -f "${BAZEL_TARGET}" "${BINARY_NAME}"
    sha256sum "${BINARY_NAME}" > "${SHA256_NAME}"

    if [ -n "${DST}" ]; then
      # Copy it to the bucket.
      echo "Copying ${BINARY_NAME} ${SHA256_NAME} to ${DST}/"
      gsutil cp "${BINARY_NAME}" "${SHA256_NAME}" "${DST}/"
    fi
  fi
done

# Exit early to skip wasm build
if [ "${BUILD_ENVOY_BINARY_ONLY}" -eq 1 ]; then
  exit 0
fi

# Build and publish Wasm plugins
extensions=(stats metadata_exchange attributegen)
TMP_WASM=$(mktemp -d -t wasm-plugins-XXXXXXXXXX)
trap 'rm -rf ${TMP_WASM}' EXIT
make build_wasm
if [ -n "${DST}" ]; then
  for extension in "${extensions[@]}"; do
    # Rename the plugin file and generate sha256 for it
    WASM_NAME="${extension}-${SHA}.wasm"
    WASM_COMPILED_NAME="${extension}-${SHA}.compiled.wasm"
    WASM_PATH="${TMP_WASM}/${WASM_NAME}"
    WASM_COMPILED_PATH="${TMP_WASM}/${WASM_COMPILED_NAME}"
    SHA256_PATH="${WASM_PATH}.sha256"
    SHA256_COMPILED_PATH="${WASM_COMPILED_PATH}.sha256"
    BAZEL_TARGET="$(bazel info output_path)/k8-opt/bin/extensions/${extension}.wasm"
    BAZEL_COMPILED_TARGET="$(bazel info output_path)/k8-opt/bin/extensions/${extension}.compiled.wasm"
    cp "${BAZEL_TARGET}" "${WASM_PATH}"
    cp "${BAZEL_COMPILED_TARGET}" "${WASM_COMPILED_PATH}"
    sha256sum "${WASM_PATH}" > "${SHA256_PATH}"
    sha256sum "${WASM_COMPILED_PATH}" > "${SHA256_COMPILED_PATH}"
    
    # push wasm files and sha to the given bucket
    gsutil stat "${DST}/${WASM_NAME}" \
      && { echo "WASM file ${WASM_NAME} already exist"; continue; } \
      || echo "Pushing the WASM file ${WASM_NAME}"
    gsutil stat "${DST}/${WASM_COMPILED_NAME}" \
      && { echo "WASM file ${WASM_COMPILED_NAME} already exist"; continue; } \
      || echo "Pushing the WASM file ${WASM_COMPILED_NAME}"
    gsutil cp "${WASM_PATH}" "${SHA256_PATH}" "${DST}"
    gsutil cp "${WASM_COMPILED_PATH}" "${SHA256_COMPILED_PATH}" "${DST}"
  done
fi
