#!/bin/bash

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

set -eu

# Integration test for apple_genrule.

# This test checks that the first argument contains a file that contains
# Xcode environment variables. Since the file was generated using an
# apple_genrule using `printenv`, we enforce that the contract of providing the
# DEVELOPER_DIR and SDKROOT environment variables is maintained.

INPUT_FILE="$1"

if ! grep -Fq DEVELOPER_DIR "$INPUT_FILE"; then
  echo "FAILURE: DEVELOPER_DIR not found."
  exit 1
fi

if ! grep -Fq SDKROOT "$INPUT_FILE"; then
  echo "FAILURE: SDKROOT not found."
  exit 1
fi
