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

# Check whether any git repositories are defined.
# Git repository definition contains `commit` and `remote` fields.
if grep -nr "commit =\|remote =" --include=WORKSPACE --include=*.bzl .; then
  echo "Using git repositories is not allowed."
  echo "To ensure that all dependencies can be stored offline in distdir, only HTTP repositories are allowed."
  exit 1
fi

# Check whether workspace file has `ENVOY_SHA = "` presented.
# This is needed by release builder to resolve envoy dep sha to tag.
if ! grep -Pq "ENVOY_SHA = \"[a-zA-Z0-9]{40}\"" WORKSPACE; then
  echo "'ENVOY_SHA' is not set properly in WORKSPACE file, release builder depends on it to resolve envoy dep sha to tag."
  exit 1
fi
