#!/bin/bash

set -euxo pipefail

export CC=clang CXX=clang++

# Download the actual WORKSPACE file for Envoy
SHA=$(sed -n -e 's/OPENSSL_ENVOY_SHA *= *"\(.*\)"/\1/p' WORKSPACE)
cd ossm/vendor/envoy
curl -sfLO "https://raw.githubusercontent.com/envoyproxy/envoy-openssl/${SHA}/WORKSPACE"

# Set the bazel flags
BAZEL_STARTUP_ARGS=${BAZEL_STARTUP_ARGS:-}
BAZEL_BUILD_ARGS=${BAZEL_BUILD_ARGS:-}

BAZEL_BUILD_ARGS+=" \
--host_force_python=PY3 \
--extra_toolchains=@local_jdk//:all \
--tool_java_runtime_version=local_jdk \
--config=clang \
--verbose_failures \
--test_output=errors \
--color=no \
--//bazel:http3=false \
--define=boringssl=fips \
--build_tag_filters=-nofips \
--test_tag_filters=-nofips \
--action_env=OPENSSL_ROOT_DIR \
--test_env=ENVOY_IP_TEST_VERSIONS=v4only \
"

if [ -n "${BAZEL_REMOTE_CACHE:-}" ]; then
  BAZEL_BUILD_ARGS+=" --remote_cache=${BAZEL_REMOTE_CACHE} "
elif [ -n "${BAZEL_DISK_CACHE:-}" ]; then
  BAZEL_BUILD_ARGS+=" --disk_cache=${BAZEL_DISK_CACHE} "
fi

if [ -n "${CI:-}" ]; then
  BAZEL_BUILD_ARGS+=" --local_cpu_resources=${LOCAL_CPU_RESOURCES:-6} "
  BAZEL_BUILD_ARGS+=" --local_ram_resources=${LOCAL_RAM_RESOURCES:-12288} "
  BAZEL_BUILD_ARGS+=" --jobs=${LOCAL_JOBS:-3} "
fi

# Make sure the bazel command runs as non-root, otherwise we will get the error
# "The current user is root, please run as non-root when using the hermetic Python interpreter. See https://github.com/bazelbuild/rules_python/pull/713."
HELPER=""
if [ "${EUID}" == "0" ]; then
  HELPER="runuser -u user -- "
  chown -R user /work
fi

# Skip failing tests
SKIP=" \
-//test/common/signal:signals_test \
-//test/extensions/filters/listener/original_dst:original_dst_integration_test \
"

# Compile the tests first
${HELPER} bazel ${BAZEL_STARTUP_ARGS} build ${BAZEL_BUILD_ARGS} --build_tests_only //test/... -- ${SKIP}

# Run the tests
${HELPER} bazel ${BAZEL_STARTUP_ARGS} test ${BAZEL_BUILD_ARGS} --build_tests_only --flaky_test_attempts=3 //test/... -- ${SKIP}
