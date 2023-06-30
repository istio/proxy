#!/bin/bash
#
# Copyright 2021 Istio Authors. All Rights Reserved.
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

# This script verifies the LastFlag response flag in istio/proxy matches the one in envoyproxy/envoy 
# and fails with a non-zero exit code if they differ.

set -x

if [[ -n "${ENVOY_REPOSITORY}" ]]; then
    echo "Skipping LastFlag verification, custom envoy used"
    exit 0
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
WORKSPACE=${ROOT}/WORKSPACE
PATH_LASTFLAG_SPEC_DOWNSTREAM="${ROOT}/extensions/common/util.cc"
PATH_LASTFLAG_SPEC_UPSTREAM="stream_info.h"
ENVOY_PATH_LASTFLAG_SPEC="envoy/stream_info/stream_info.h"

ENVOY_ORG="$(grep -Pom1 "^ENVOY_ORG = \"\K[a-zA-Z-]+" "${WORKSPACE}")"
ENVOY_REPO="$(grep -Pom1 "^ENVOY_REPO = \"\K[a-zA-Z-]+" "${WORKSPACE}")"
ENVOY_SHA="$(grep -Pom1 "^ENVOY_SHA = \"\K[a-zA-Z0-9]{40}" "${WORKSPACE}")"

trap 'rm -rf ${PATH_LASTFLAG_SPEC_UPSTREAM}' EXIT
curl -sSL "https://raw.githubusercontent.com/${ENVOY_ORG}/${ENVOY_REPO}/${ENVOY_SHA}/${ENVOY_PATH_LASTFLAG_SPEC}" > "${PATH_LASTFLAG_SPEC_UPSTREAM}"

# Extract the LastFlag specification from both sources and trim all white spaces.

if grep -q "^\s*LastFlag\s*=.*$" "${PATH_LASTFLAG_SPEC_DOWNSTREAM}"
then
    DOWNSTREAM_LASTFLAG=$(grep -m1 "^\s*LastFlag\s*=.*$" "${PATH_LASTFLAG_SPEC_DOWNSTREAM}"|tr -d '[:space:]')
else
    echo "istio/proxy: LastFlag not specified in ${PATH_LASTFLAG_SPEC_DOWNSTREAM}"
    exit 1
fi

if grep -q "^\s*LastFlag\s*=.*$" ${PATH_LASTFLAG_SPEC_UPSTREAM}
then
    UPSTREAM_LASTFLAG=$(grep -m1 "^\s*LastFlag\s*=.*$" ${PATH_LASTFLAG_SPEC_UPSTREAM}|tr -d '[:space:]')
else
    echo "envoyproxy/envoy: LastFlag not specified in https://raw.githubusercontent.com/${ENVOY_ORG}/${ENVOY_REPO}/${ENVOY_SHA}/${ENVOY_PATH_LASTFLAG_SPEC}"
    exit 1
fi

# The trailing comma is optional and will be removed from the extracted values before comparison.

if [[ "${DOWNSTREAM_LASTFLAG:${#DOWNSTREAM_LASTFLAG}-1}" == "," ]]
then
    DOWNSTREAM_LASTFLAG="${DOWNSTREAM_LASTFLAG:0:${#DOWNSTREAM_LASTFLAG}-1}"
fi

if [[ "${UPSTREAM_LASTFLAG:${#UPSTREAM_LASTFLAG}-1}" == "," ]]
then
   UPSTREAM_LASTFLAG="${UPSTREAM_LASTFLAG:0:${#UPSTREAM_LASTFLAG}-1}"
fi

if [[ "$DOWNSTREAM_LASTFLAG" != "$UPSTREAM_LASTFLAG" ]]
then
    echo "The LastFlag specification for downstream and upstream differs. Downstream is ${DOWNSTREAM_LASTFLAG}. Upstream is ${UPSTREAM_LASTFLAG}."
    exit 1
fi
