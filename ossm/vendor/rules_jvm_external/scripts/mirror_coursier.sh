#!/usr/bin/env bash

# Copyright 2019 The Bazel Authors. All rights reserved.
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
# This script mirrors the Coursier standalone jar to the Bazel mirror
# on Google Cloud Storage for redundancy. The original artifacts are
# hosted on https://github.com/coursier/coursier/releases.

set -xeuo pipefail

readonly dest_filename=$1; shift;
readonly coursier_cli_jar="external/_main~_repo_rules~coursier_cli/file/downloaded"
chmod u+x $coursier_cli_jar

# Upload Coursier to Bazel/Google-managed GCS
# -n for no-clobber, so we don't overwrite existing files.
# -v prints the URI after a successful upload.
gsutil cp -v -n $coursier_cli_jar \
  gs://bazel-mirror/coursier_cli/$dest_filename.jar
