# Copyright 2022 The Bazel Authors. All rights reserved.
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
"""Implementation of py_test rule."""

load(":attributes.bzl", "AGNOSTIC_TEST_ATTRS")
load(":common.bzl", "maybe_add_test_execution_info")
load(
    ":py_executable.bzl",
    "create_executable_rule_builder",
    "py_executable_impl",
)

_BAZEL_PY_TEST_ATTRS = {
    # This *might* be a magic attribute to help C++ coverage work. There's no
    # docs about this; see TestActionBuilder.java
    "_collect_cc_coverage": attr.label(
        default = "@bazel_tools//tools/test:collect_cc_coverage",
        executable = True,
        cfg = "exec",
    ),
    # This *might* be a magic attribute to help C++ coverage work. There's no
    # docs about this; see TestActionBuilder.java
    "_lcov_merger": attr.label(
        default = configuration_field(fragment = "coverage", name = "output_generator"),
        cfg = "exec",
        executable = True,
    ),
}

def _py_test_impl(ctx):
    providers = py_executable_impl(
        ctx = ctx,
        is_test = True,
        inherited_environment = ctx.attr.env_inherit,
    )
    maybe_add_test_execution_info(providers, ctx)
    return providers

def create_test_rule_builder():
    builder = create_executable_rule_builder(
        implementation = _py_test_impl,
        test = True,
    )
    builder.attrs.update(AGNOSTIC_TEST_ATTRS)
    builder.attrs.update(_BAZEL_PY_TEST_ATTRS)
    return builder

py_test = create_test_rule_builder().build()
