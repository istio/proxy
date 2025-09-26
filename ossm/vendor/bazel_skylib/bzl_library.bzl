# Copyright 2017 The Bazel Authors. All rights reserved.
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

"""Skylib module containing a library rule for aggregating rules files."""

# buildifier: disable=bzl-visibility
load(
    "//rules/private:bzl_library.bzl",
    _StarlarkLibraryInfo = "StarlarkLibraryInfo",
    _bzl_library = "bzl_library",
)

StarlarkLibraryInfo = _StarlarkLibraryInfo

def bzl_library(name, **kwargs):
    """Wrapper for bzl_library.

    Args:
        name: name
        **kwargs: see the generated doc for rules/private/bzl_library.
    """

    # buildifier: disable=unused-variable
    _ = kwargs.pop("compatible_with", None)
    _ = kwargs.pop("exec_compatible_with", None)
    _ = kwargs.pop("features", None)
    _ = kwargs.pop("target_compatible_with", None)
    _bzl_library(
        name = name,
        compatible_with = [],
        exec_compatible_with = [],
        features = [],
        target_compatible_with = [],
        **kwargs
    )
