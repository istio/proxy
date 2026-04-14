#!/bin/bash

set -x
set -euo pipefail

# Change to the top dir
cd "$(dirname "$0")/.."

# Build with libstdc++ rather than libc++ because the bssl-compat prefixer tool
# is linked against some of the LLVM libraries which require libstdc++
export ENVOY_STDLIB=libstdc++

# Tell the upstream run_envoy_docker.sh script to use our builder image
export ENVOY_BUILD_IMAGE=$(grep ENVOY_BUILD_IMAGE .github/workflows/envoy-openssl.yml | awk '{print $2}')
# Hand off to the upstream run_envoy_docker.sh script
exec ./ci/run_envoy_docker.sh "$@"
