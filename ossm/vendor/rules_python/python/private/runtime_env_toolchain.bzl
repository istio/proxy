# Copyright 2019 The Bazel Authors. All rights reserved.
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
"""Definitions related to the Python toolchain."""

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("//python:py_runtime.bzl", "py_runtime")
load("//python:py_runtime_pair.bzl", "py_runtime_pair")
load("//python/cc:py_cc_toolchain.bzl", "py_cc_toolchain")
load(":py_exec_tools_toolchain.bzl", "py_exec_tools_toolchain")
load(":toolchain_types.bzl", "EXEC_TOOLS_TOOLCHAIN_TYPE", "PY_CC_TOOLCHAIN_TYPE", "TARGET_TOOLCHAIN_TYPE")

_IS_EXEC_TOOLCHAIN_ENABLED = Label("//python/config_settings:is_exec_tools_toolchain_enabled")

def define_runtime_env_toolchain(name):
    """Defines the runtime_env Python toolchain.

    This is a minimal suite of toolchains that provided limited functionality.
    They're mostly only useful to aid migration off the builtin
    `@bazel_tools//tools/python:autodetecting_toolchain` toolchain.

    NOTE: This was previously called the "autodetecting" toolchain, but was
    renamed to better reflect its behavior, since it doesn't autodetect
    anything.

    Args:
        name: The name of the toolchain to introduce.
    """
    base_name = name.replace("_toolchain", "")

    py_runtime(
        name = "_runtime_env_py3_runtime",
        interpreter = "//python/private:runtime_env_toolchain_interpreter.sh",
        python_version = "PY3",
        stub_shebang = "#!/usr/bin/env python3",
        visibility = ["//visibility:private"],
        tags = ["manual"],
    )

    # This is a dummy runtime whose interpreter_path triggers the native rule
    # logic to use the legacy behavior on Windows.
    # TODO(#7844): Remove this target.
    py_runtime(
        name = "_magic_sentinel_runtime",
        interpreter_path = "/_magic_pyruntime_sentinel_do_not_use",
        python_version = "PY3",
        visibility = ["//visibility:private"],
        tags = ["manual"],
    )

    py_runtime_pair(
        name = "_runtime_env_py_runtime_pair",
        py3_runtime = select({
            # If we're on windows, inject the sentinel to tell native rule logic
            # that we attempted to use the runtime_env toolchain and need to
            # switch back to legacy behavior.
            # TODO(#7844): Remove this hack.
            "@platforms//os:windows": ":_magic_sentinel_runtime",
            "//conditions:default": ":_runtime_env_py3_runtime",
        }),
        visibility = ["//visibility:public"],
        tags = ["manual"],
    )

    native.toolchain(
        name = name,
        toolchain = ":_runtime_env_py_runtime_pair",
        toolchain_type = TARGET_TOOLCHAIN_TYPE,
        visibility = ["//visibility:public"],
    )

    py_exec_tools_toolchain(
        name = "_runtime_env_py_exec_tools_toolchain_impl",
        precompiler = Label("//tools/precompiler:precompiler"),
        visibility = ["//visibility:private"],
        tags = ["manual"],
    )
    native.toolchain(
        name = base_name + "_py_exec_tools_toolchain",
        toolchain = "_runtime_env_py_exec_tools_toolchain_impl",
        toolchain_type = EXEC_TOOLS_TOOLCHAIN_TYPE,
        target_settings = [_IS_EXEC_TOOLCHAIN_ENABLED],
        visibility = ["//visibility:public"],
    )
    cc_library(
        name = "_empty_cc_lib",
        visibility = ["//visibility:private"],
        tags = ["manual"],
    )
    py_cc_toolchain(
        name = "_runtime_env_py_cc_toolchain_impl",
        headers = ":_empty_cc_lib",
        libs = ":_empty_cc_lib",
        python_version = "0.0",
        tags = ["manual"],
    )
    native.toolchain(
        name = base_name + "_py_cc_toolchain",
        toolchain = ":_runtime_env_py_cc_toolchain_impl",
        toolchain_type = PY_CC_TOOLCHAIN_TYPE,
        visibility = ["//visibility:public"],
    )
