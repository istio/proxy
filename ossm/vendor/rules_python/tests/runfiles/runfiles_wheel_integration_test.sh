#!/usr/bin/env bash
# Copyright 2023 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Manual test, run outside of Bazel, to check that our runfiles wheel should be functional
# for users who install it from pypi.
set -o errexit 

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

bazel 2>/dev/null build --stamp --embed_label=1.2.3 //python/runfiles:wheel
wheelpath=$SCRIPTPATH/../../$(bazel 2>/dev/null cquery --output=files //python/runfiles:wheel)
PYTHONPATH=$wheelpath python3 -c 'import importlib;print(importlib.import_module("runfiles"))'
