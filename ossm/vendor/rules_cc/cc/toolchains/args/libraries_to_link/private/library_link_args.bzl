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
"""Helper macros for declaring library link arguments."""

load("//cc/toolchains:nested_args.bzl", "cc_nested_args")

def macos_force_load_library_args(name, variable):
    """A helper for declaring -force_load argument expansion for a library.

    This creates an argument expansion that will expand to -Wl,-force_load,<library>
    if the library should be linked as a whole archive.

    Args:
      name: The name of the rule.
      variable: The variable to expand.
    """
    cc_nested_args(
        name = name,
        nested = [
            ":{}_force_load_library".format(name),
            ":{}_no_force_load_library".format(name),
        ],
    )
    cc_nested_args(
        name = name + "_no_force_load_library",
        requires_false = "//cc/toolchains/variables:libraries_to_link.is_whole_archive",
        args = ["{library}"],
        format = {
            "library": variable,
        },
    )
    cc_nested_args(
        name = name + "_force_load_library",
        requires_true = "//cc/toolchains/variables:libraries_to_link.is_whole_archive",
        args = ["-Wl,-force_load,{library}"],
        format = {
            "library": variable,
        },
    )

def library_link_args(name, library_type, from_variable, iterate_over_variable = False):
    """A helper for declaring a library to link.

    For most platforms, this expands something akin to the following:

        cc_nested_args(
            name = "foo",
            requires_equal = "//cc/toolchains/variables:libraries_to_link.type",
            requires_equal_value = "interface_library",
            iterate_over = None,
            args = ["{library}"],
            format = {
                "library": //cc/toolchains/variables:libraries_to_link.name,
            },
        )

    For macos, this expands to a more complex cc_nested_args structure that
    handles the -force_load flag.

    Args:
      name: The name of the rule.
      library_type: The type of the library.
      from_variable: The variable to expand.
      iterate_over_variable: Whether to iterate over the variable.
    """
    native.alias(
        name = name,
        actual = select({
            "@platforms//os:macos": ":macos_{}".format(name),
            "//conditions:default": ":generic_{}".format(name),
        }),
    )
    cc_nested_args(
        name = "generic_{}".format(name),
        requires_equal = "//cc/toolchains/variables:libraries_to_link.type",
        requires_equal_value = library_type,
        iterate_over = from_variable if iterate_over_variable else None,
        args = ["{library}"],
        format = {
            "library": from_variable,
        },
    )
    cc_nested_args(
        name = "macos_{}".format(name),
        requires_equal = "//cc/toolchains/variables:libraries_to_link.type",
        requires_equal_value = library_type,
        iterate_over = from_variable if iterate_over_variable else None,
        nested = [":{}_maybe_force_load".format(name)],
    )
    macos_force_load_library_args(
        name = "{}_maybe_force_load".format(name),
        variable = from_variable,
    )
