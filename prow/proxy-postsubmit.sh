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
# shellcheck disable=SC1090,SC1091
source "${WD}/proxy-common.inc"

if [[ $(command -v gcloud) ]]; then
  gcloud auth configure-docker -q
elif [[ $(command -v docker-credential-gcr) ]]; then
  docker-credential-gcr configure-docker
else
  echo "No credential helpers found, push to docker may not function properly"
fi

GCS_BUILD_BUCKET="${GCS_BUILD_BUCKET:-istio-build}"

echo 'Create and push artifacts'
make push_release RELEASE_GCS_PATH="gs://${GCS_BUILD_BUCKET}/proxy"
