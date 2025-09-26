# Copyright 2025 The Bazel Authors. All rights reserved.
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
"""A test runner toolchain for C++ tests that executes the binary directly."""

load("@bazel_features//:features.bzl", "bazel_features")

visibility("private")

def _has_any_target_constraint(ctx, constraints):
    for constraint in constraints:
        constraint_value = constraint[platform_common.ConstraintValueInfo]
        if ctx.target_platform_has_constraint(constraint_value):
            return True
    return False

def _default_test_runner_func(ctx, binary_info, processed_environment):
    providers = [
        DefaultInfo(
            executable = binary_info.executable,
            files = binary_info.files,
            runfiles = binary_info.runfiles,
        ),
        RunEnvironmentInfo(
            environment = processed_environment,
            inherited_environment = ctx.attr.env_inherit,
        ),
    ]

    if _has_any_target_constraint(ctx, ctx.attr._apple_constraints):
        # When built for Apple platforms, require the execution to be on a Mac.
        providers.append(testing.ExecutionInfo({"requires-darwin": ""}))

    return providers

def _default_test_runner_impl(_ctx):
    return [
        platform_common.ToolchainInfo(
            cc_test_info = struct(
                get_runner = struct(
                    args = {},
                    func = _default_test_runner_func,
                ),
                linkopts = [],
                linkstatic = False,
            ),
        ),
    ]

default_test_runner = rule(_default_test_runner_impl)

def compat_toolchain(*, name, **kwargs):
    if not bazel_features.toolchains.has_use_target_platform_constraints:
        kwargs.pop("use_target_platform_constraints", None)

    native.toolchain(
        name = name,
        **kwargs
    )
