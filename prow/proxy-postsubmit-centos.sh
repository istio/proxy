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

WD=$(dirname "$0")
WD=$(cd "$WD" || exit 1 ; pwd)

########################################
# Postsubmit script triggered by Prow. #
########################################

# Do not use RBE execution with Ubuntu toolchain, but still use RBE cache.
export BAZEL_BUILD_RBE_JOBS=0

# shellcheck disable=SC1090
source "${WD}/proxy-common.inc"

if [[ -n "${GOOGLE_APPLICATION_CREDENTIALS:-}" ]]; then
  echo "Detected GOOGLE_APPLICATION_CREDENTIALS, configuring Docker..." >&2
  gcloud auth configure-docker
fi

GCS_BUILD_BUCKET="${GCS_BUILD_BUCKET:-istio-build}"

echo 'Create and push artifacts'
make push_release_centos RELEASE_GCS_PATH="gs://${GCS_BUILD_BUCKET}/proxy"
