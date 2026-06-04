#!/bin/bash

set -exo pipefail

DIR=$(cd "$(dirname "$0")" ; pwd -P)

# shellcheck disable=SC1091
source "${DIR}/common.sh"

# Build Envoy
time bazel_build //:envoy

echo "Build succeeded. Binary generated:"
bazel-bin/envoy --version

# Run tests
time bazel_test //...

export ENVOY_PATH=bazel-bin/envoy
export GO111MODULE=on

# shellcheck disable=SC2046
time go test -timeout=30m -p=1 -parallel=1 $(go list ./...)
