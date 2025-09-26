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
    doc = "Internal Rule implementing py_common API.",
)

def _py_common_api_typedef():
    """The py_common API implementation.

    An instance of this object is obtained using {obj}`py_common.get()`
    """

def _merge_py_infos(transitive, *, direct = []):
    """Merge PyInfo objects into a single PyInfo.

    This is a convenience wrapper around {obj}`PyInfoBuilder.merge_all`. For
    more control over merging PyInfo objects, use {obj}`PyInfoBuilder`.

    Args:
        transitive: {type}`list[PyInfo]` The PyInfo objects with info
            considered indirectly provided by something (e.g. via
            its deps attribute).
        direct: {type}`list[PyInfo]` The PyInfo objects that are
            considered directly provided by something (e.g. via
            the srcs attribute).

    Returns:
        {type}`PyInfo` A PyInfo containing the merged values.
    """
    builder = PyInfoBuilder.new()
    builder.merge_all(transitive, direct = direct)
    return builder.build()

# Exposed for doc generation, not directly used.
# buildifier: disable=name-conventions
PyCommonApi = struct(
    TYPEDEF = _py_common_api_typedef,
    merge_py_infos = _merge_py_infos,
    PyInfoBuilder = PyInfoBuilder.new,
)
