# Copyright 2025 The Bazel Authors. All rights reserved.
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

"""Implementation of the rules to access the underlying Python interpreter."""

load("@bazel_skylib//lib:paths.bzl", "paths")
load("//python:py_runtime_info.bzl", "PyRuntimeInfo")
load(":common.bzl", "runfiles_root_path")
load(":sentinel.bzl", "SentinelInfo")
load(":toolchain_types.bzl", "TARGET_TOOLCHAIN_TYPE")

def _interpreter_binary_impl(ctx):
    if SentinelInfo in ctx.attr.binary:
        toolchain = ctx.toolchains[TARGET_TOOLCHAIN_TYPE]
        runtime = toolchain.py3_runtime
    else:
        runtime = ctx.attr.binary[PyRuntimeInfo]

    # NOTE: We name the output filename after the underlying file name
    # because of things like pyenv: they use $0 to determine what to
    # re-exec. If it's not a recognized name, then they fail.
    if runtime.interpreter:
        # In order for this to work both locally and remotely, we create a
        # shell script here that re-exec's into the real interpreter. Ideally,
        # we'd just use a symlink, but that breaks under certain conditions. If
        # we use a ctx.actions.symlink(target=...) then it fails under remote
        # execution. If we use ctx.actions.symlink(target_path=...) then it
        # behaves differently inside the runfiles tree and outside the runfiles
        # tree.
        #
        # This currently does not work on Windows. Need to find a way to enable
        # that.
        executable = ctx.actions.declare_file(runtime.interpreter.basename)
        ctx.actions.expand_template(
            template = ctx.file._template,
            output = executable,
            substitutions = {
                "%target_file%": runfiles_root_path(ctx, runtime.interpreter.short_path),
            },
            is_executable = True,
        )
    else:
        executable = ctx.actions.declare_symlink(paths.basename(runtime.interpreter_path))
        ctx.actions.symlink(output = executable, target_path = runtime.interpreter_path)

    return [
        DefaultInfo(
            executable = executable,
            runfiles = ctx.runfiles([executable], transitive_files = runtime.files).merge_all([
                ctx.attr._bash_runfiles[DefaultInfo].default_runfiles,
            ]),
        ),
    ]

interpreter_binary = rule(
    implementation = _interpreter_binary_impl,
    toolchains = [TARGET_TOOLCHAIN_TYPE],
    executable = True,
    attrs = {
        "binary": attr.label(
            mandatory = True,
        ),
        "_bash_runfiles": attr.label(
            default = "@bazel_tools//tools/bash/runfiles",
        ),
        "_template": attr.label(
            default = "//python/private:interpreter_tmpl.sh",
            allow_single_file = True,
        ),
    },
)
