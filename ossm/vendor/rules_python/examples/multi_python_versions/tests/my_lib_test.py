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

import os
import sys

import libs.my_lib as my_lib

workspace_version = f"{sys.version_info.major}_{sys.version_info.minor}"
bzlmod_version = f"{sys.version_info.major}{sys.version_info.minor}"

if not my_lib.websockets_is_for_python_version(
    workspace_version
) and not my_lib.websockets_is_for_python_version(bzlmod_version):
    print(
        "expected package for Python version is different than returned\n"
        f"expected either {workspace_version} or {bzlmod_version}\n"
        f"but got {my_lib.websockets.__file__}"
    )
    sys.exit(1)
