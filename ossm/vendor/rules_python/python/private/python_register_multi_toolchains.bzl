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

"""This file contains repository rules and macros to support toolchain registration.
"""

# NOTE @aignas 2024-10-07:  we are not importing this from `@pythons_hub` because of this
# leading to a backwards incompatible change - the `//python:repositories.bzl` is loading
# from this file and it will cause a circular import loop and an error. If the users in
# WORKSPACE world want to override the `minor_mapping`, they will have to pass an argument.
load("//python:versions.bzl", "MINOR_MAPPING")
load(":python_register_toolchains.bzl", "python_register_toolchains")
load(":toolchains_repo.bzl", "multi_toolchain_aliases")

def python_register_multi_toolchains(
        name,
        python_versions,
        default_version = None,
        minor_mapping = None,
        **kwargs):
    """Convenience macro for registering multiple Python toolchains.

    Args:
        name: {type}`str` base name for each name in {obj}`python_register_toolchains` call.
        python_versions: {type}`list[str]` the Python versions.
        default_version: {type}`str` the default Python version. If not set,
            the first version in python_versions is used.
        minor_mapping: {type}`dict[str, str]` mapping between `X.Y` to `X.Y.Z`
            format. Defaults to the value in `//python:versions.bzl`.
        **kwargs: passed to each {obj}`python_register_toolchains` call.
    """
    if len(python_versions) == 0:
        fail("python_versions must not be empty")

    minor_mapping = minor_mapping or MINOR_MAPPING

    if not default_version:
        default_version = python_versions.pop(0)
    for python_version in python_versions:
        if python_version == default_version:
            # We register the default version lastly so that it's not picked first when --platforms
            # is set with a constraint during toolchain resolution. This is due to the fact that
            # Bazel will match the unconstrained toolchain if we register it before the constrained
            # ones.
            continue
        python_register_toolchains(
            name = name + "_" + python_version.replace(".", "_"),
            python_version = python_version,
            set_python_version_constraint = True,
            minor_mapping = minor_mapping,
            **kwargs
        )
    python_register_toolchains(
        name = name + "_" + default_version.replace(".", "_"),
        python_version = default_version,
        set_python_version_constraint = False,
        minor_mapping = minor_mapping,
        **kwargs
    )

    multi_toolchain_aliases(
        name = name,
        python_versions = {
            python_version: name + "_" + python_version.replace(".", "_")
            for python_version in (python_versions + [default_version])
        },
        minor_mapping = minor_mapping,
    )
