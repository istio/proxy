#!/bin/bash

set -euo pipefail
set -x

readonly binary="%{binary}s"
expected_platform="MACOS"
if [[ "$PLATFORM_TYPE" == "ios" && "$BUILD_TYPE" == "device" ]]; then
  expected_platform="IOS"
elif [[ "$PLATFORM_TYPE" == "ios" && "$BUILD_TYPE" == "simulator" ]]; then
  expected_platform="IOSSIMULATOR"
elif [[ "$PLATFORM_TYPE" == "visionos" && "$BUILD_TYPE" == "device" ]]; then
  expected_platform="XROS"
elif [[ "$PLATFORM_TYPE" == "visionos" && "$BUILD_TYPE" == "simulator" ]]; then
  expected_platform="XROSSIMULATOR"
elif [[ "$PLATFORM_TYPE" == "watchos" && "$BUILD_TYPE" == "device" ]]; then
  expected_platform="WATCHOS"
elif [[ "$PLATFORM_TYPE" == "watchos" && "$BUILD_TYPE" == "simulator" ]]; then
  expected_platform="WATCHSIMULATOR"
fi

platforms=$(otool -lv "$binary" | grep "platform " | sort -u || true)
if ! echo "$platforms" | grep -q "platform $expected_platform"; then
  echo "error: binary $binary does not contain platform $expected_platform, got: '$platforms'" >&2
  exit 1
fi

lipo_output=$(lipo -info "$binary")
expected_cpus=${CPU//,/ }
expected_cpus=${expected_cpus//sim_/}
expected_cpus=${expected_cpus//device_/}
if ! echo "$lipo_output" | grep -q "$expected_cpus"; then
  echo "error: binary $binary does not contain CPU $CPU, got: '$lipo_output"
  exit 1
fi
