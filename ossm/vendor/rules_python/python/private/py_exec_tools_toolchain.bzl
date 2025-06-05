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

"""Rule that defines a toolchain for build tools."""

load("@bazel_skylib//lib:paths.bzl", "paths")
load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")
load(":py_exec_tools_info.bzl", "PyExecToolsInfo")
load(":sentinel.bzl", "SentinelInfo")
load(":toolchain_types.bzl", "TARGET_TOOLCHAIN_TYPE")

def _py_exec_tools_toolchain_impl(ctx):
    extra_kwargs = {}
    if ctx.attr._visible_for_testing[BuildSettingInfo].value:
        extra_kwargs["toolchain_label"] = ctx.label

    exec_interpreter = ctx.attr.exec_interpreter
    if SentinelInfo in ctx.attr.exec_interpreter:
        exec_interpreter = None

    return [platform_common.ToolchainInfo(
        exec_tools = PyExecToolsInfo(
            exec_interpreter = exec_interpreter,
            precompiler = ctx.attr.precompiler,
        ),
        **extra_kwargs
    )]

py_exec_tools_toolchain = rule(
    implementation = _py_exec_tools_toolchain_impl,
    doc = """
Provides a toolchain for build time tools.

This provides `ToolchainInfo` with the following attributes:
* `exec_tools`: {type}`PyExecToolsInfo`
* `toolchain_label`: {type}`Label` _only present when `--visibile_for_testing=True`
  for internal testing_. The rule's label; this allows identifying what toolchain
  implmentation was selected for testing purposes.
""",
    attrs = {
        "exec_interpreter": attr.label(
            default = "//python/private:current_interpreter_executable",
            cfg = "exec",
            doc = """
An interpreter that is directly usable in the exec configuration

If not specified, the interpreter from {obj}`//python:toolchain_type` will
be used.

To disable, specify the special target {obj}`//python:none`; the raw value `None`
will use the default.

:::{note}
This is only useful for `ctx.actions.run` calls that _directly_ invoke the
interpreter, which is fairly uncommon and low level. It is better to use a
`cfg="exec"` attribute that points to a `py_binary` rule instead, which will
handle all the necessary transitions and runtime setup to invoke a program.
:::

See {obj}`PyExecToolsInfo.exec_interpreter` for further docs.
""",
        ),
        "precompiler": attr.label(
            allow_files = True,
            cfg = "exec",
            doc = "See {obj}`PyExecToolsInfo.precompiler`",
        ),
        "_visible_for_testing": attr.label(
            default = "//python/private:visible_for_testing",
        ),
    },
)

def _current_interpreter_executable_impl(ctx):
    toolchain = ctx.toolchains[TARGET_TOOLCHAIN_TYPE]
    runtime = toolchain.py3_runtime

    # NOTE: We name the output filename after the underlying file name
    # because of things like pyenv: they use $0 to determine what to
    # re-exec. If it's not a recognized name, then they fail.
    if runtime.interpreter:
        executable = ctx.actions.declare_file(runtime.interpreter.basename)
        ctx.actions.symlink(output = executable, target_file = runtime.interpreter, is_executable = True)
    else:
        executable = ctx.actions.declare_symlink(paths.basename(runtime.interpreter_path))
        ctx.actions.symlink(output = executable, target_path = runtime.interpreter_path)
    return [
        toolchain,
        DefaultInfo(
            executable = executable,
            runfiles = ctx.runfiles([executable], transitive_files = runtime.files),
        ),
    ]

current_interpreter_executable = rule(
    implementation = _current_interpreter_executable_impl,
    toolchains = [TARGET_TOOLCHAIN_TYPE],
    executable = True,
)
