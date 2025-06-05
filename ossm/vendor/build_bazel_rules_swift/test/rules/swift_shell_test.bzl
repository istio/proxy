# Copyright 2023 The Bazel Authors. All rights reserved.
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

"""Test rule for verifying output of Swift binaries."""

load("@bazel_skylib//lib:shell.bzl", "shell")

def _swift_shell_test_impl(ctx):
    """Implementation of the swift_shell_test rule."""

    test_executable = ctx.attr.target_under_test.files_to_run.executable

    runfiles = ctx.runfiles(
        files = [test_executable],
    ).merge_all([
        ctx.attr.target_under_test.default_runfiles,
        ctx.attr._tool.default_runfiles,
    ])

    output_script = ctx.actions.declare_file("{}_test_script".format(ctx.label.name))
    ctx.actions.expand_template(
        template = ctx.file._runner_template,
        output = output_script,
        substitutions = {
            "%executable%": ctx.workspace_name + "/" + test_executable.short_path,
            "%expected_return_code%": str(ctx.attr.expected_return_code),
            "%expected_logs%": shell.array_literal(ctx.attr.expected_logs),
            "%not_expected_logs%": shell.array_literal(ctx.attr.not_expected_logs),
        },
        is_executable = True,
    )

    return [
        DefaultInfo(
            executable = output_script,
            runfiles = runfiles,
        ),
        ctx.attr.target_under_test[OutputGroupInfo],
    ]

swift_shell_test = rule(
    implementation = _swift_shell_test_impl,
    attrs = {
        "expected_return_code": attr.int(
            doc = "The expected return code from the target under test",
        ),
        "expected_logs": attr.string_list(
            mandatory = False,
            doc = "Logs that are expected to be emitted",
        ),
        "not_expected_logs": attr.string_list(
            mandatory = False,
            doc = "Logs that are not expected to be emitted",
        ),
        "target_under_test": attr.label(
            mandatory = True,
            doc = "The Swift binary whose outputs to test.",
            providers = [DefaultInfo],
        ),
        "_tool": attr.label(
            default = Label("@bazel_tools//tools/bash/runfiles"),
        ),
        "_runner_template": attr.label(
            allow_single_file = True,
            default = Label("//test/rules:swift_shell_runner.sh.template"),
        ),
    },
    test = True,
)
