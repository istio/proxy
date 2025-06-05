# Copyright 2016 The Bazel Authors. All rights reserved.
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

# This becomes the BUILD file for @local_config_cc// under non-BSD unixes.

load(":cc_toolchain_config.bzl", "cc_toolchain_config")
load(":armeabi_cc_toolchain_config.bzl", "armeabi_cc_toolchain_config")
load("@rules_cc//cc/toolchains:cc_toolchain.bzl", "cc_toolchain")
load("@rules_cc//cc/toolchains:cc_toolchain_suite.bzl", "cc_toolchain_suite")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

cc_library(name = "empty_lib")

# Label flag for extra libraries to be linked into every binary.
# TODO(bazel-team): Support passing flag multiple times to build a list.
label_flag(
    name = "link_extra_libs",
    build_setting_default = ":empty_lib",
)

# The final extra library to be linked into every binary target. This collects
# the above flag, but may also include more libraries depending on config.
cc_library(
    name = "link_extra_lib",
    deps = [
        ":link_extra_libs",
    ],
)

cc_library(
    name = "malloc",
)

filegroup(
    name = "empty",
    srcs = [],
)

filegroup(
    name = "cc_wrapper",
    srcs = ["cc_wrapper.sh"],
)

filegroup(
    name = "validate_static_library",
    srcs = ["validate_static_library.sh"],
)

filegroup(
    name = "deps_scanner_wrapper",
    srcs = ["deps_scanner_wrapper.sh"],
)
filegroup(
    name = "compiler_deps",
    srcs = glob(["extra_tools/**"], allow_empty = True) + [%{cc_compiler_deps}],
)

# This is the entry point for --crosstool_top.  Toolchains are found
# by lopping off the name of --crosstool_top and searching for
# the "${CPU}" entry in the toolchains attribute.
cc_toolchain_suite(
    name = "toolchain",
    toolchains = {
        "%{name}|%{compiler}": ":cc-compiler-%{name}",
        "%{name}": ":cc-compiler-%{name}",
        "armeabi-v7a|compiler": ":cc-compiler-armeabi-v7a",
        "armeabi-v7a": ":cc-compiler-armeabi-v7a",
    },
)

cc_toolchain(
    name = "cc-compiler-%{name}",
    toolchain_identifier = "%{cc_toolchain_identifier}",
    toolchain_config = ":%{cc_toolchain_identifier}",
    all_files = ":compiler_deps",
    ar_files = ":compiler_deps",
    as_files = ":compiler_deps",
    compiler_files = ":compiler_deps",
    dwp_files = ":empty",
    linker_files = ":compiler_deps",
    objcopy_files = ":empty",
    strip_files = ":empty",
    supports_header_parsing = 1,
    supports_param_files = 1,
    module_map = %{modulemap},
)

cc_toolchain_config(
    name = "%{cc_toolchain_identifier}",
    cpu = "%{target_cpu}",
    compiler = "%{compiler}",
    toolchain_identifier = "%{cc_toolchain_identifier}",
    host_system_name = "%{host_system_name}",
    target_system_name = "%{target_system_name}",
    target_libc = "%{target_libc}",
    abi_version = "%{abi_version}",
    abi_libc_version = "%{abi_libc_version}",
    cxx_builtin_include_directories = [%{cxx_builtin_include_directories}],
    tool_paths = {%{tool_paths}},
    compile_flags = [%{compile_flags}],
    opt_compile_flags = [%{opt_compile_flags}],
    dbg_compile_flags = [%{dbg_compile_flags}],
    conly_flags = [%{conly_flags}],
    cxx_flags = [%{cxx_flags}],
    link_flags = [%{link_flags}],
    link_libs = [%{link_libs}],
    opt_link_flags = [%{opt_link_flags}],
    unfiltered_compile_flags = [%{unfiltered_compile_flags}],
    coverage_compile_flags = [%{coverage_compile_flags}],
    coverage_link_flags = [%{coverage_link_flags}],
    supports_start_end_lib = %{supports_start_end_lib},
    extra_flags_per_feature = %{extra_flags_per_feature},
)

# Android tooling requires a default toolchain for the armeabi-v7a cpu.
cc_toolchain(
    name = "cc-compiler-armeabi-v7a",
    toolchain_identifier = "stub_armeabi-v7a",
    toolchain_config = ":stub_armeabi-v7a",
    all_files = ":empty",
    ar_files = ":empty",
    as_files = ":empty",
    compiler_files = ":empty",
    dwp_files = ":empty",
    linker_files = ":empty",
    objcopy_files = ":empty",
    strip_files = ":empty",
    supports_param_files = 1,
)

armeabi_cc_toolchain_config(name = "stub_armeabi-v7a")
