#!/bin/bash
#
# Copyright 2017 Istio Authors. All Rights Reserved.
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

WD=$(dirname $0)
WD=$(cd $WD; pwd)
ROOT=$(dirname $WD)

########################################
# Postsubmit script triggered by Prow. #
########################################

# Exit immediately for non zero status
set -e
# Check unset variables
set -u
# Print commands
set -x

gcloud auth activate-service-account --key-file=/etc/service-account/service-account.json

GOPATH=/go
ROOT=/go/src
rm -f "${HOME}/.bazelrc"

export BAZEL_BUILD_ARGS="--local_ram_resources=12288 --local_cpu_resources=8 --verbose_failures"

echo 'Create and push artifacts'
scripts/release-binary.sh
ARTIFACTS_DIR="gs://istio-artifacts/proxy/${GIT_SHA}/artifacts/debs" make artifacts
