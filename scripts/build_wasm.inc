#!/bin/bash

# Copyright Istio Authors
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

# Build Wasm modules 
build_wasm() {
    root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
    workspace=${root}/WORKSPACE
    wasm_sdk_tag="$(grep -Pom1 "^ENVOY_SHA = \"\K[a-zA-Z0-9]{40}" "${workspace}")"
    wasm_sdk_image=${WASM_SDK_IMAGE:=gcr.io/istio-testing/wasmsdk}

    docker run -e uid="$(id -u)" -e gid="$(id -g)" -v $PWD:/work -w /work -v $(realpath $PWD/../../extensions):/work/extensions ${wasm_sdk_image}:${wasm_sdk_tag} bash /build_wasm.sh
    rmdir extensions
}
