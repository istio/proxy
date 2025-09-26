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


set -o errexit -o nounset -o pipefail

# VERSION_PY_BINARY is a space separate list of the executable and its main
# py file. We just want the executable.
bin=($VERSION_PY_BINARY)
bin="${bin[@]//*.py}"
version_py_binary=$($bin)

if [[ "${version_py_binary}" != "${VERSION_CHECK}" ]]; then
    echo >&2 "expected version '${VERSION_CHECK}' is different than returned '${version_py_binary}'"
    exit 1
fi
