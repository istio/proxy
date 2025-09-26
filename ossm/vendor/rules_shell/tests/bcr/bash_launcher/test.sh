#!/usr/bin/env bash
# Copyright 2024 The Bazel Authors. All rights reserved.
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

bin_path="$(rlocation "rules_shell_tests/bash_launcher/bin.sh")"
if [[ ! -x "${bin_path}" ]]; then
  echo "Expected '${bin_path}' to be an executable"
  exit 1
fi

greeting=$("${bin_path}")
if [[ "${greeting}" != "hello from rules_shell" ]]; then
  echo "Expected 'hello from rules_shell', got '${greeting}'"
  exit 1
fi
