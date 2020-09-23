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


# Update the Envoy SHA in istio/proxy WORKSPACE with the first argument
set -e

UPDATE_BRANCH=${UPDATE_BRANCH:-"master"}

SCRIPTPATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ROOTDIR=$(dirname "${SCRIPTPATH}")
cd "${ROOTDIR}"

# Get ENVOY_SHA256
SHA256=$(wget https://github.com/istio/envoy/archive/$1.tar.gz && sha256sum $1.tar.gz)

# Update Commit date and release branch
sed -i '{ s/"Commit date": .*/ "Commit date": "'"$DATE"'"/  }' WORKSPACE
sed -i '{ s/"Branch": .*/ "Branch": "'"$UPDATE_BRANCH"'"/  }' WORKSPACE

# Update the dependency in istio/proxy WORKSPACE
sed -i '{ s/"ENVOY_SHA" = .*/"ENVOY_SHA" = "'"$1"'"/  }' WORKSPACE
sed -i '{ s/"ENVOY_SHA256" = .*/"ENVOY_SHA256" = "'"$SHA256"'"/  }' WORKSPACE
