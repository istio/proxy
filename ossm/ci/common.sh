#!/bin/bash

export CC=clang CXX=clang++ ENVOY_OPENSSL=1

ARCH=$(uname -p)
if [ "${ARCH}" = "ppc64le" ]; then
  ARCH="ppc"
fi
export ARCH

OUTPUT_TO_IGNORE="\
INFO: From|\
Fixing bazel-out|\
cache:INFO:  - ok|\
processwrapper-sandbox.*execroot.*io_istio_proxy|\
proto is unused\
"

COMMON_FLAGS="\
    --config=release \
    --config=${ARCH} \
"

if [ -n "${BAZEL_REMOTE_CACHE}" ]; then
  COMMON_FLAGS+=" --remote_cache=${BAZEL_REMOTE_CACHE} "
elif [ -n "${BAZEL_DISK_CACHE}" ]; then
  COMMON_FLAGS+=" --disk_cache=${BAZEL_DISK_CACHE} "
fi

if [ -n "${CI}" ]; then
  COMMON_FLAGS+=" --config=ci-config "

  # Throttle resources to work for our CI environemt
  LOCAL_CPU_RESOURCES="${LOCAL_CPU_RESOURCES:-6}"
  LOCAL_RAM_RESOURCES="${LOCAL_RAM_RESOURCES:-12288}"
  LOCAL_JOBS="${LOCAL_JOBS:-3}"

  COMMON_FLAGS+=" --local_cpu_resources=${LOCAL_CPU_RESOURCES} "
  COMMON_FLAGS+=" --local_ram_resources=${LOCAL_RAM_RESOURCES} "
  COMMON_FLAGS+=" --jobs=${LOCAL_JOBS} "
fi

function bazel_build() {
  bazel build \
    ${COMMON_FLAGS} \
    "${@}" \
  2>&1 | grep --line-buffered -v -E "${OUTPUT_TO_IGNORE}"
}

function bazel_test() {
  bazel test \
    ${COMMON_FLAGS} \
    --build_tests_only \
    "${@}" \
  2>&1 | grep --line-buffered -v -E "${OUTPUT_TO_IGNORE}"
}

# Fix path to the vendor deps
sed -i "s|=/work/|=$(pwd)/|" ossm/bazelrc-vendor
