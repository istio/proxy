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
    -p push the wasm sdk container built from the envoy SHA. Must use with `-b`
    -d The bucket name to store the generated wasm files."
  exit 1
}

BUILD_CONTAINER=0
PUSH_CONTAINER=0
DST_BUCKET=""

while getopts bpcd: arg ; do
  case "${arg}" in
    b) BUILD_CONTAINER=1;;
    p) PUSH_DOCKER_IMAGE=1;;
    d) DST_BUCKET="${OPTARG}";;
    *) usage;;
  esac
done

# Get SHA of envoy-wasm repo
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
WORKSPACE=${ROOT}/WORKSPACE
ENVOY_SHA="$(grep -Pom1 "^ENVOY_SHA = \"\K[a-zA-Z0-9]{40}" "${WORKSPACE}")"
ENVOY_ORG="$(grep -Pom1 "^ENVOY_ORG = \"\K[a-zA-Z-]+" "${WORKSPACE}")"
ENVOY_REPO="$(grep -Pom1 "^ENVOY_REPO = \"\K[a-zA-Z-]+" "${WORKSPACE}")"
WASM_SDK_IMAGE=${WASM_SDK_IMAGE:=gcr.io/istio-testing/wasmsdk}
export WASM_SDK_TAG=${ENVOY_SHA}

# Try pull wasm builder image.
docker pull ${WASM_SDK_IMAGE}:${WASM_SDK_TAG} || echo "${WASM_SDK_IMAGE}:${WASM_SDK_TAG} does not exist"

# If image does not exist, try build it
if [[ "$(docker images -q ${WASM_SDK_IMAGE}:${WASM_SDK_TAG} 2> /dev/null)" == "" ]]; then
  if [[ ${BUILD_CONTAINER} == 0 ]]; then
    echo "no builder image to compile wasm. Add `-b` option to create the builder image"
    exit 1
  fi
  # Clone envoy-wasm repo and checkout to that SHA
  TMP_DIR=$(mktemp -d -t ${ENVOY_REPO}-XXXXXXXXXX)
  trap "rm -rf ${TMP_DIR}" EXIT

  if [[ -z "${ENVOY_DIR}" ]]; then
    cd ${TMP_DIR}
    git clone https://github.com/${ENVOY_ORG}/${ENVOY_REPO}
    cd ${ENVOY_REPO}
  else
    # ENVOY_DIR is absolute path of local envoy dir used for Wasm build.
    cp -r ${ENVOY_DIR} ${TMP_DIR}
    cd ${TMP_DIR}/$(basename "${ENVOY_DIR}")
  fi

  # Check out to envoy SHA
  git checkout ${ENVOY_SHA}

  # Rebuild and push
  cd api/wasm/cpp && docker build -t ${WASM_SDK_IMAGE}:${WASM_SDK_TAG} -f Dockerfile-sdk .
  if [[ ${PUSH_DOCKER_IMAGE} == 1 ]]; then
    docker push ${WASM_SDK_IMAGE}:${WASM_SDK_TAG} || echo "fail to push to ${WASM_SDK_IMAGE} hub"
  fi
fi

# Regenerate all wasm plugins and compare diffs
cd ${ROOT}
find . -name "*.wasm" -type f -delete
make build_wasm

echo "Destination bucket: ${DST_BUCKET}"
if [ -n "${DST_BUCKET}" ]; then
  cd ${ROOT}
  # Get SHA of proxy repo
  SHA="$(git rev-parse --verify HEAD)"
  TMP_WASM=$(mktemp -d -t wasm-plugins-XXXXXXXXXX)
  trap "rm -rf ${TMP_WASM}" EXIT
  for i in `find . -name "*.wasm" -type f`; do
    ls -lh ${i}
    # Get name of the plugin
    PLUGIN_NAME=$(basename $(dirname ${i}))
    # Rename the plugin file and generate sha256 for it
    WASM_NAME="${PLUGIN_NAME}-${SHA}.wasm"
    WASM_PATH="${TMP_WASM}/${WASM_NAME}"
    SHA256_PATH="${WASM_PATH}.sha256"
    cp ${i} ${WASM_PATH}
    sha256sum "${WASM_PATH}" > "${SHA256_PATH}"
    
    # push wasm files and sha to the given bucket
    gsutil stat "${DST_BUCKET}/${WASM_NAME}" \
      && { echo "WASM file ${WASM_NAME} already exist"; continue; } \
      || echo "Pushing the WASM file ${WASM_NAME}"
    gsutil cp "${WASM_PATH}" "${SHA256_PATH}" "${DST_BUCKET}"
  done
fi
