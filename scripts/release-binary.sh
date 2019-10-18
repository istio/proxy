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

function usage() {
  echo "$0
    -d  The bucket name to store proxy binary (optional).
    -i  Skip Ubuntu Xenial check. DO NOT USE THIS FOR RELEASED BINARIES.
        Cannot be used together with -d option."
  exit 1
}

while getopts d:i arg ; do
  case "${arg}" in
    d) DST="${OPTARG}";;
    i) CHECK=0;;
    *) usage;;
  esac
done

echo "Destination bucket: $DST"

if [ "${DST}" == "none" ]; then
  DST=""
fi

# Make sure the release binaries are built on x86_64 Ubuntu 16.04 (Xenial)
if [ "${CHECK}" -eq 1 ]; then
  UBUNTU_RELEASE=${UBUNTU_RELEASE:-$(lsb_release -c -s)}
  [[ "${UBUNTU_RELEASE}" == 'xenial' ]] || { echo 'Must run on Ubuntu 16.04 (Xenial).'; exit 1; }
  [[ "$(uname -m)" == 'x86_64' ]] || { echo 'Must run on x86_64.'; exit 1; }
elif [ -n "${DST}" ]; then
  echo "The -i option is not allowed together with -d option."
  exit 1
fi

# Symlinks don't work, use full path as a temporary workaround.
# See: https://github.com/istio/istio/issues/15714 for details.
# k8-opt is the output directory for x86_64 optimized builds (-c opt, so --config=release-symbol and --config=release).
BAZEL_OUT="$(bazel info output_path)/k8-opt/bin"

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

# Build the release binary.
BINARY_NAME="${HOME}/envoy-alpha-${SHA}.tar.gz"
SHA256_NAME="${HOME}/envoy-alpha-${SHA}.sha256"
bazel build ${BAZEL_BUILD_ARGS} --config=release //src/envoy:envoy_tar
BAZEL_TARGET="${BAZEL_OUT}/src/envoy/envoy_tar.tar.gz"
cp -f "${BAZEL_TARGET}" "${BINARY_NAME}"
sha256sum "${BINARY_NAME}" > "${SHA256_NAME}"

if [ -n "${DST}" ]; then
  # Copy it to the bucket.
  echo "Copying ${BINARY_NAME} ${SHA256_NAME} to ${DST}/"
  gsutil cp "${BINARY_NAME}" "${SHA256_NAME}" "${DST}/"
fi

# Build the release package.
BINARY_NAME="${HOME}/istio-proxy-${SHA}.deb"
SHA256_NAME="${HOME}/istio-proxy-${SHA}.sha256"
bazel build ${BAZEL_BUILD_ARGS} --config=release //tools/deb:istio-proxy
BAZEL_TARGET="${BAZEL_OUT}/tools/deb/istio-proxy.deb"
cp -f "${BAZEL_TARGET}" "${BINARY_NAME}"
sha256sum "${BINARY_NAME}" > "${SHA256_NAME}"

if [ -n "${DST}" ]; then
  # Copy it to the bucket.
  echo "Copying ${BINARY_NAME} ${SHA256_NAME} to ${DST}/"
  gsutil cp "${BINARY_NAME}" "${SHA256_NAME}" "${DST}/"
fi

# Build the release binary with symbols.
BINARY_NAME="${HOME}/envoy-symbol-${SHA}.tar.gz"
SHA256_NAME="${HOME}/envoy-symbol-${SHA}.sha256"
bazel build ${BAZEL_BUILD_ARGS} --config=release-symbol //src/envoy:envoy_tar
BAZEL_TARGET="${BAZEL_OUT}/src/envoy/envoy_tar.tar.gz"
cp -f "${BAZEL_TARGET}" "${BINARY_NAME}"
sha256sum "${BINARY_NAME}" > "${SHA256_NAME}"

if [ -n "${DST}" ]; then
  # Copy it to the bucket.
  echo "Copying ${BINARY_NAME} ${SHA256_NAME} to ${DST}/"
  gsutil cp "${BINARY_NAME}" "${SHA256_NAME}" "${DST}/"
fi

# Build the release binary with symbols and AddressSanitizer (ASan).
# NOTE: libc++ is dynamically linked in this build.
BINARY_NAME="${HOME}/envoy-asan-${SHA}.tar.gz"
SHA256_NAME="${HOME}/envoy-asan-${SHA}.sha256"
bazel build ${BAZEL_BUILD_ARGS} ${BAZEL_CONFIG_ASAN} --config=release-symbol //src/envoy:envoy_tar
BAZEL_TARGET="${BAZEL_OUT}/src/envoy/envoy_tar.tar.gz"
cp -f "${BAZEL_TARGET}" "${BINARY_NAME}"
sha256sum "${BINARY_NAME}" > "${SHA256_NAME}"

if [ -n "${DST}" ]; then
  # Copy it to the bucket.
  echo "Copying ${BINARY_NAME} ${SHA256_NAME} to ${DST}/"
  gsutil cp "${BINARY_NAME}" "${SHA256_NAME}" "${DST}/"
fi

# Symlinks don't work, use full path as a temporary workaround.
# See: https://github.com/istio/istio/issues/15714 for details.
# k8-dbg is the output directory for x86_64 debug builds (-c dbg).
BAZEL_OUT="$(bazel info output_path)/k8-dbg/bin"

# Build the debug binary.
BINARY_NAME="${HOME}/envoy-debug-${SHA}.tar.gz"
SHA256_NAME="${HOME}/envoy-debug-${SHA}.sha256"
bazel build ${BAZEL_BUILD_ARGS} -c dbg //src/envoy:envoy_tar
BAZEL_TARGET="${BAZEL_OUT}/src/envoy/envoy_tar.tar.gz"
cp -f "${BAZEL_TARGET}" "${BINARY_NAME}"
sha256sum "${BINARY_NAME}" > "${SHA256_NAME}"

if [ -n "${DST}" ]; then
  # Copy it to the bucket.
  echo "Copying ${BINARY_NAME} ${SHA256_NAME} to ${DST}/"
  gsutil cp "${BINARY_NAME}" "${SHA256_NAME}" "${DST}/"
fi

# Build the debug package.
BINARY_NAME="${HOME}/istio-proxy-debug-${SHA}.deb"
SHA256_NAME="${HOME}/istio-proxy-debug-${SHA}.sha256"
bazel build ${BAZEL_BUILD_ARGS} -c dbg //tools/deb:istio-proxy
BAZEL_TARGET="${BAZEL_OUT}/tools/deb/istio-proxy.deb"
cp -f "${BAZEL_TARGET}" "${BINARY_NAME}"
exit
sha256sum "${BINARY_NAME}" > "${SHA256_NAME}"

if [ -n "${DST}" ]; then
  # Copy it to the bucket.
  echo "Copying ${BINARY_NAME} ${SHA256_NAME} to ${DST}/"
  gsutil cp "${BINARY_NAME}" "${SHA256_NAME}" "${DST}/"
fi
