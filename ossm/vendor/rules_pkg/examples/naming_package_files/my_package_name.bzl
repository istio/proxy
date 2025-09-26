# Copyright 2020 The Bazel Authors. All rights reserved.
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

"""Example rules to show package naming techniques."""

load("@rules_cc//cc:find_cc_toolchain.bzl", "find_cc_toolchain")
load("@rules_pkg//pkg:providers.bzl", "PackageVariablesInfo")

def _basic_naming_impl(ctx):
    values = {}

    # Copy attributes from the rule to the provider
    values["product_name"] = ctx.attr.product_name
    values["version"] = ctx.attr.version
    values["revision"] = ctx.attr.revision
    values["special_build"] = ctx.attr.special_build

    # Add some well known variables from the rule context.
    values["target_cpu"] = ctx.var.get("TARGET_CPU")
    values["compilation_mode"] = ctx.var.get("COMPILATION_MODE")

    return PackageVariablesInfo(values = values)

#
# A rule to inject variables from the build file into package names.
#
basic_naming = rule(
    implementation = _basic_naming_impl,
    attrs = {
        "product_name": attr.string(
            doc = "Placeholder for our final product name.",
        ),
        "revision": attr.string(
            doc = "Placeholder for our release revision.",
        ),
        "version": attr.string(
            doc = "Placeholder for our release version.",
        ),
        "special_build": attr.string(
            doc = "Indicates that we have built with a 'special' option.",
        ),
    },
)

def _names_from_toolchains_impl(ctx):
    values = {}

    # TODO(https://github.com/bazelbuild/bazel/issues/7260): Switch from
    # calling find_cc_toolchain to direct lookup via the name.
    # cc_toolchain = ctx.toolchains["@rules_cc//cc:toolchain_type"]
    cc_toolchain = find_cc_toolchain(ctx)

    # compiler is uninformative. Use the name of the executable
    values["compiler"] = cc_toolchain.compiler_executable.split("/")[-1]
    values["cc_cpu"] = cc_toolchain.cpu
    values["libc"] = cc_toolchain.libc

    values["compilation_mode"] = ctx.var.get("COMPILATION_MODE")

    return PackageVariablesInfo(values = values)

#
# Extracting variables from the toolchain to use in the package name.
#
names_from_toolchains = rule(
    implementation = _names_from_toolchains_impl,
    # Going forward, the preferred way to depend on a toolchain through the
    # toolchains attribute. The current C++ toolchains, however, are still not
    # using toolchain resolution, so we have to depend on the toolchain
    # directly.
    # TODO(https://github.com/bazelbuild/bazel/issues/7260): Delete the
    # _cc_toolchain attribute.
    attrs = {
        "_cc_toolchain": attr.label(
            default = Label(
                "@rules_cc//cc:current_cc_toolchain",
            ),
        ),
    },
    toolchains = ["@rules_cc//cc:toolchain_type"],
    incompatible_use_toolchain_transition = True,
)

#
# Using a command line build setting to name a package.
#
def _name_part_from_command_line_naming_impl(ctx):
    values = {"name_part": ctx.build_setting_value}

    # Just pass the value from the command line through. An implementation
    # could also perform validation, such as done in
    # https://github.com/bazelbuild/bazel-skylib/blob/master/rules/common_settings.bzl
    return PackageVariablesInfo(values = values)

#
# Creating this build_setting defines a flag the user can set.
#
name_part_from_command_line = rule(
    implementation = _name_part_from_command_line_naming_impl,
    # Note that the default value comes from the rule instantiation.
    build_setting = config.string(flag = True),
)
