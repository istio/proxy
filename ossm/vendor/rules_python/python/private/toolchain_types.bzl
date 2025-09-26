# Copyright 2024 The Bazel Authors. All rights reserved.
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
"""Labels to identify toolchain types.

This is a separate file because things needing the toolchain types (in
particular, toolchain() registrations) shouldn't need to load the entire
implementation of the toolchain.
"""

TARGET_TOOLCHAIN_TYPE = Label("//python:toolchain_type")
EXEC_TOOLS_TOOLCHAIN_TYPE = Label("//python:exec_tools_toolchain_type")
PY_CC_TOOLCHAIN_TYPE = Label("//python/cc:toolchain_type")
