# Copyright 2021 The Bazel Authors. All rights reserved.
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

# Proof-of-concept example showing how to write a custom C++ toolchain.
#
# Important documentation:
#
# - https://docs.bazel.build/versions/master/platforms-intro.html#c
# - https://docs.bazel.build/versions/master/tutorial/cc-toolchain-config.html
# - https://docs.bazel.build/versions/master/be/c-cpp.html#cc_toolchain
#
# There are two ways to select C++ toolchains:
#
#  - NEW (USE IF POSSIBLE): with the --platforms flag
#  - LEGACY: with the --crosstool_top and --cpu flags
#
# See https://docs.bazel.build/versions/master/platforms-intro.html#c for details.
#
# This example demonstrates both approaches.

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc/toolchains:cc_toolchain.bzl", "cc_toolchain")
load("@rules_cc//cc/toolchains:cc_toolchain_suite.bzl", "cc_toolchain_suite")

# Load the Starlark logic defining the toolchain's behavior. For example: what
# program runs to compile a source file and how its command line is
# constructed. See toolchain_config.bzl for details.
load(":toolchain_config.bzl", "cc_toolchain_config")

# The library we want to build. Building this calls two C++ actions: compile (.cc ->
# .o) and archive (.o -> .a).
cc_library(
    name = "buildme",
    srcs = ["buildme.cc"],
)

# This example intentionally makes the cc_toolchain_config definition
# simple. You could alternative add attributes to support multiple
# cc_toolchain_config targets with finer customization.
cc_toolchain_config(
    name = "toolchain_semantics",
)

# Register the toolchain with Bazel. Most of these attribute just tell Bazel
# where to find the files needed to run C++ commands. The toolchain_config
# attribute registers the behavior specification declared above.
cc_toolchain(
    name = "my_custom_toolchain",
    all_files = ":toolchain_files",
    ar_files = ":toolchain_files",
    compiler_files = ":toolchain_files",
    dwp_files = ":toolchain_files",
    linker_files = ":toolchain_files",
    objcopy_files = ":toolchain_files",
    strip_files = ":toolchain_files",
    toolchain_config = ":toolchain_semantics",
)

filegroup(
    name = "toolchain_files",
    srcs = [
        "sample_compiler",
        "sample_linker",
    ],
)

# Implements legacy toolchain selection.
#
# Setting --crosstool_top here registers the set of available
# toolchains. Setting --cpu to one of the toolchain attribute's keys selects a
#toolchain.
cc_toolchain_suite(
    name = "legacy_selector",
    toolchains = {
        "k8": ":my_custom_toolchain",
    },
)

# Implements platform-based (recommended) toolchain selection.
#
# See https://docs.bazel.build/versions/master/platforms-intro.html. The main
# differences are:
#
#  1. --cpu / --crosstool_top are replaced by a platform() definition with
#       much more customizable properties. For example, a platform can specify
#       OS, device type (server, phone, tablet) or custom hardware extensions.
#  2. All languages can support platform-based toolchains. A single --platforms
#       value can choose C++, Python, Scala, and all other toolchains in your
#       build. This is especially useful for multi-language builds.
#  3. Platforms  support features like incompatible target skipping:
#       https://docs.bazel.build/versions/master/platforms.html#skipping-incompatible-targets.
toolchain(
    name = "platform_based_toolchain",
    # Trigger this toolchain for x86-compatible platforms.
    # See https://github.com/bazelbuild/platforms.
    target_compatible_with = ["@platforms//cpu:x86_64"],
    # Register this toolchain with platforms.
    toolchain = ":my_custom_toolchain",
    # The public interface for all C++ toolchains. Starlark rules that use C++
    # access the toolchain through this interface.
    toolchain_type = "@bazel_tools//tools/cpp:toolchain_type",
)

# Define a platform matching any x86-compatible toolchain. See
# https://docs.bazel.build/versions/master/platforms.html.
platform(
    name = "x86_platform",
    constraint_values = ["@platforms//cpu:x86_64"],
)
