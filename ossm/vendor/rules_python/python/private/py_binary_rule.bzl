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
"""Rule implementation of py_binary for Bazel."""

load(":attributes.bzl", "AGNOSTIC_BINARY_ATTRS")
load(
    ":py_executable.bzl",
    "create_executable_rule_builder",
    "py_executable_impl",
)

_COVERAGE_ATTRS = {
    # Magic attribute to help C++ coverage work. There's no
    # docs about this; see TestActionBuilder.java
    "_collect_cc_coverage": attr.label(
        default = "@bazel_tools//tools/test:collect_cc_coverage",
        executable = True,
        cfg = "exec",
    ),
    # Magic attribute to make coverage work. There's no
    # docs about this; see TestActionBuilder.java
    "_lcov_merger": attr.label(
        default = configuration_field(fragment = "coverage", name = "output_generator"),
        executable = True,
        cfg = "exec",
    ),
}

def _py_binary_impl(ctx):
    return py_executable_impl(
        ctx = ctx,
        is_test = False,
        inherited_environment = [],
    )

def create_binary_rule_builder():
    builder = create_executable_rule_builder(
        implementation = _py_binary_impl,
        executable = True,
    )
    builder.attrs.update(AGNOSTIC_BINARY_ATTRS)
    builder.attrs.update(_COVERAGE_ATTRS)
    return builder

py_binary = create_binary_rule_builder().build()
