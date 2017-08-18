#!/bin/bash

# Builds a docker image with istio-proxy deb included, to be used for testing.

# Script requires a working docker on the test machine
# It is run in the proxy dir, will create a docker image with proxy deb installed


bazel build tools/deb:istio-proxy

PROJECT="istio-testing"
DATE_PART=$(date +"%Y%m%d")
SHA_PART=$(git show -q HEAD --pretty=format:%h)
DOCKER_TAG="${DATE_PART}-${SHA_PART}"
IMAGE_NAME="gcr.io/${PROJECT}/rawvm:${DOCKER_TAG}"

DOCKER_IMAGE=${DOCKER_IMAGE:-$IMAGE_NAME}

BAZEL_TARGET="bazel-bin/tools/deb/"

cp -f $BAZEL_TARGET/istio-proxy_*_amd64.deb tools/deb/test/istio-proxy_amd64.deb
docker build -f tools/deb/test/Dockerfile -t "${DOCKER_IMAGE}" tools/deb/test


