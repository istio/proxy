#!/bin/bash

# Copyright 2020 Istio Authors
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


# Update the Envoy SHA in istio/proxy WORKSPACE with the first argument (aka ENVOY_SHA) and
# the second argument (aka ENVOY_SHA commit date)

# Exit immediately for non zero status
set -e
# Check unset variables
set -u
# Print commands
set -x

# Update to main as envoyproxy/proxy has updated.
UPDATE_BRANCH=${UPDATE_BRANCH:-"main"}

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
WORKSPACE=${ROOT}/WORKSPACE

ENVOY_ORG="$(grep -Pom1 "^ENVOY_ORG = \"\K[a-zA-Z-]+" "${WORKSPACE}")"
ENVOY_REPO="$(grep -Pom1 "^ENVOY_REPO = \"\K[a-zA-Z-]+" "${WORKSPACE}")"

# get latest commit for specified org/repo
LATEST_SHA="$(git ls-remote https://github.com/"${ENVOY_ORG}"/"${ENVOY_REPO}" "refs/heads/$UPDATE_BRANCH" | awk '{ print $1}')"
DATE=$(curl -s -H "Accept: application/vnd.github.v3+json" https://api.github.com/repos/"${ENVOY_ORG}""/""${ENVOY_REPO}"/commits/"${LATEST_SHA}" | jq '.commit.committer.date')
DATE=$(echo "${DATE/\"/}" | cut -d'T' -f1)

# Get ENVOY_SHA256
URL="https://github.com/${ENVOY_ORG}/${ENVOY_REPO}/archive/${LATEST_SHA}.tar.gz"
GETSHA=$(wget "${URL}" && sha256sum "${LATEST_SHA}".tar.gz | awk '{ print $1 }')
SHAArr=("${GETSHA}")
SHA256=${SHAArr[0]}
rm "${LATEST_SHA}".tar.gz

# Update ENVOY_SHA commit date
sed -i "s/Commit date: .*/Commit date: ${DATE}/" "${WORKSPACE}"

# Update the dependency in istio/proxy WORKSPACE
sed -i 's/ENVOY_SHA = .*/ENVOY_SHA = "'"$LATEST_SHA"'"/' "${WORKSPACE}"
sed -i 's/ENVOY_SHA256 = .*/ENVOY_SHA256 = "'"$SHA256"'"/' "${WORKSPACE}"

# Update .bazelversion and envoy.bazelrc
curl -sSL "https://raw.githubusercontent.com/${ENVOY_ORG}/${ENVOY_REPO}/${LATEST_SHA}/.bazelversion" > .bazelversion
curl -sSL "https://raw.githubusercontent.com/${ENVOY_ORG}/${ENVOY_REPO}/${LATEST_SHA}/.bazelrc" > envoy.bazelrc
