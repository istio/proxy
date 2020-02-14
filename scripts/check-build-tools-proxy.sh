#!/bin/bash
#
# Copyright 2018 Istio Authors. All Rights Reserved.
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

set -eu

# Check whether the proxy build-tools image tag in Makefile.overrides is the same as
# build-tools image tag in Makefile.
proxy_build_tools_tag=$(grep -oP 'gcr.io/istio-testing/build-tools-proxy:\K([-0-9a-zA-Z]+)' Makefile.overrides.mk)
build_tools_tag=$(grep -oP 'IMAGE_VERSION\s*(\?|:)?=\s*\K([-0-9a-zA-Z]+)' Makefile)

if [[ $proxy_build_tools_tag != $build_tools_tag ]]; then
    echo "proxy build-tools tag $proxy_build_tools_tag is different from build-tools tag $build_tools_tag"
    exit 1
fi

