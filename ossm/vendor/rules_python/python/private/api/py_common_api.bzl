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
"""Implementation of py_api."""

load("//python/private:py_info.bzl", "PyInfoBuilder")
load("//python/private/api:api.bzl", "ApiImplInfo")

def _py_common_api_impl(ctx):
    _ = ctx  # @unused
    return [ApiImplInfo(impl = PyCommonApi)]

py_common_api = rule(
    implementation = _py_common_api_impl,
    doc = "Rule implementing py_common API.",
)

def _merge_py_infos(transitive, *, direct = []):
    builder = PyInfoBuilder()
    builder.merge_all(transitive, direct = direct)
    return builder.build()

# Exposed for doc generation, not directly used.
# buildifier: disable=name-conventions
PyCommonApi = struct(
    merge_py_infos = _merge_py_infos,
    PyInfoBuilder = PyInfoBuilder,
)
