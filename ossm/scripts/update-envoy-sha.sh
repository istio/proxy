#!/bin/bash
# Copyright Red Hat, Inc. All rights reserved.
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

set -xeuo pipefail

function cleanup() {
  rm -rf "${WORKDIR}"
}

function init() {
  WORKDIR="$(mktemp -d)"
  trap cleanup EXIT
}

function get_envoy_sha() {
  local branch
  branch="${BRANCH:-release/v1.34}"

  SHA=$(git ls-remote https://github.com/envoyproxy/envoy-openssl.git "refs/heads/${branch}" | cut -f 1)
}

function get_envoy_sha_256() {
  pushd "${WORKDIR}" >/dev/null
  curl -sfLO "https://github.com/envoyproxy/envoy-openssl/archive/${SHA}.tar.gz"
  SHA256=$(sha256sum "${SHA}.tar.gz" | awk '{print $1}')
  popd >/dev/null
}

function main() {
  local today

  init

  today=$(date +%D)
  get_envoy_sha
  get_envoy_sha_256

  sed -i "s|^# Commit date: .*|# Commit date: ${today}|" WORKSPACE
  sed -i "s|^OPENSSL_ENVOY_SHA = .*|OPENSSL_ENVOY_SHA = \"${SHA}\"|" WORKSPACE
  sed -i "s|^OPENSSL_ENVOY_SHA256 = .*|OPENSSL_ENVOY_SHA256 = \"${SHA256}\"|" WORKSPACE
}

main
