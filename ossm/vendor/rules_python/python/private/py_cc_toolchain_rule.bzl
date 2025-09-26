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

"""Implementation of py_cc_toolchain rule.

NOTE: This is a beta-quality feature. APIs subject to change until
https://github.com/bazel-contrib/rules_python/issues/824 is considered done.
"""

load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")
load("@rules_cc//cc/common:cc_info.bzl", "CcInfo")
load(":py_cc_toolchain_info.bzl", "PyCcToolchainInfo")

def _py_cc_toolchain_impl(ctx):
    if ctx.attr.libs:
        libs = struct(
            providers_map = {
                "CcInfo": ctx.attr.libs[CcInfo],
                "DefaultInfo": ctx.attr.libs[DefaultInfo],
            },
        )
    else:
        libs = None

    py_cc_toolchain = PyCcToolchainInfo(
        headers = struct(
            providers_map = {
                "CcInfo": ctx.attr.headers[CcInfo],
                "DefaultInfo": ctx.attr.headers[DefaultInfo],
            },
        ),
        libs = libs,
        python_version = ctx.attr.python_version,
    )
    extra_kwargs = {}
    if ctx.attr._visible_for_testing[BuildSettingInfo].value:
        extra_kwargs["toolchain_label"] = ctx.label
    return [platform_common.ToolchainInfo(
        py_cc_toolchain = py_cc_toolchain,
        **extra_kwargs
    )]

py_cc_toolchain = rule(
    implementation = _py_cc_toolchain_impl,
    attrs = {
        "headers": attr.label(
            doc = ("Target that provides the Python headers. Typically this " +
                   "is a cc_library target."),
            providers = [CcInfo],
            mandatory = True,
        ),
        "libs": attr.label(
            doc = ("Target that provides the Python runtime libraries for linking. " +
                   "Typically this is a cc_library target of `.so` files."),
            providers = [CcInfo],
        ),
        "python_version": attr.string(
            doc = "The Major.minor Python version, e.g. 3.11",
            mandatory = True,
        ),
        "_visible_for_testing": attr.label(
            default = "//python/private:visible_for_testing",
        ),
    },
    doc = """\
A toolchain for a Python runtime's C/C++ information (e.g. headers)

This rule carries information about the C/C++ side of a Python runtime, e.g.
headers, shared libraries, etc.

This provides `ToolchainInfo` with the following attributes:
* `py_cc_toolchain`: {type}`PyCcToolchainInfo`
* `toolchain_label`: {type}`Label` _only present when `--visibile_for_testing=True`
  for internal testing_. The rule's label; this allows identifying what toolchain
  implmentation was selected for testing purposes.
""",
)
