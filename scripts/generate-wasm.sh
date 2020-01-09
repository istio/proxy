#!/bin/bash
#
# Copyright 2020 Istio Authors. All Rights Reserved.
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

function usage() {
  echo "$0
    -b build the wasm sdk image base on ENVOY SHA if it does not exist in `gcr.io/istio-testing` HUB.
       If the image already exist in the HUB, this will be noop.
       The container will be used to compile wasm files.
    -p push the wasm sdk container built from the envoy SHA. Must use with `-c`
    -c controls whether to check diff of generated wasm files."
  exit 1
}

BUILD_CONTAINER=0
PUSH_CONTAINER=0
CHECK_DIFF=0

while getopts bpc arg ; do
  case "${arg}" in
    b) BUILD_CONTAINER=1;;
    p) PUSH_DOCKER_IMAGE=1;;
    c) CHECK_DIFF=1;;
    *) usage;;
  esac
done

# Get SHA of envoy-wasm repo
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
WORKSPACE=${ROOT}/WORKSPACE
ENVOY_SHA="$(grep -Pom1 "^ENVOY_SHA = \"\K[a-zA-Z0-9]{40}" "${WORKSPACE}")"
IMAGE=gcr.io/istio-testing/wasmsdk
TAG=${ENVOY_SHA}

# Try pull wasm builder image.
docker pull ${IMAGE}:${TAG} || echo "${IMAGE}:${TAG} does not exist"

# If image does not exist, try build it
if [[ "$(docker images -q ${IMAGE}:${TAG} 2> /dev/null)" == "" ]]; then
  if [[ ${BUILD_CONTAINER} == 0 ]]; then
    echo "no builder image to compile wasm. Add `-c` option to create the builder image"
    exit 1
  fi
  # Clone envoy-wasm repo and checkout to that SHA
  TMP_DIR=$(mktemp -d -t envoy-wasm-XXXXXXXXXX)
  trap "rm -rf ${TMP_DIR}" EXIT

  # Check out to envoy SHA
  cd ${TMP_DIR}
  git clone https://github.com/envoyproxy/envoy-wasm
  cd envoy-wasm
  git checkout ${ENVOY_SHA}

  # Rebuild and push
  cd api/wasm/cpp && docker build -t ${IMAGE}:${TAG} -f Dockerfile-sdk .
  if [[ ${PUSH_DOCKER_IMAGE} == 1 ]]; then
    docker push ${IMAGE}:${TAG} || "fail to push to gcr.io/istio-testing hub"
  fi
fi

# Regenerate all wasm plugins and compare diffs
# Tag image to v2, which is what used by all build wasm script.
docker tag ${IMAGE}:${TAG} ${IMAGE}:v2
cd ${ROOT}
find . -name "*.wasm" -type f -delete
make build_wasm

if [[ ${CHECK_DIFF} == 1 ]]; then
  if [[ -n "$(git status --porcelain 2>/dev/null)" ]]; then
    echo "wasm files are out of dated and need to be regenerated, run './scripts/generate-wasm.sh -b' to regenerate them"
    exit 1
  else
    echo "wasm files are up to dated"
  fi
fi
