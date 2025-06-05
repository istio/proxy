#!/usr/bin/env bash

# @license
# Copyright 2017 The Bazel Authors. All rights reserved.

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.

# You may obtain a copy of the License at
#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This is a workaround for bazelbuild/bazel#6293. Since Bazel 0.18.0, Bazel
# expects tests to have an "$lcov_merger' or "_lcov_merger" attribute that
# points to an executable. If this is missing, the test driver fails.

# Copied from https://github.com/bazelbuild/rules_go/blob/64c97b54ea2918fc7f1a59d68cd27d1fdb0bd663/go/tools/builders/lcov_merger.sh

exit 0