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

"""Public entry point for PyInfo."""

load("@rules_python_internal//:rules_python_config.bzl", "config")
load("//python/private:py_info.bzl", _starlark_PyInfo = "PyInfo")
load("//python/private:reexports.bzl", "BuiltinPyInfo")

PyInfo = _starlark_PyInfo if config.enable_pystar or BuiltinPyInfo == None else BuiltinPyInfo
