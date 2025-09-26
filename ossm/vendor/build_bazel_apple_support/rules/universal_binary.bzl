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

"""Implementation for macOS universal binary rule."""

load("@bazel_skylib//lib:dicts.bzl", "dicts")
load("//lib:apple_support.bzl", "apple_support")
load("//lib:lipo.bzl", "lipo")
load("//lib:transitions.bzl", "macos_universal_transition")

def _universal_binary_impl(ctx):
    inputs = [
        binary.files.to_list()[0]
        for binary in ctx.split_attr.binary.values()
    ]

    if not inputs:
        fail("Target (%s) `binary` label ('%s') does not provide any " +
             "file for universal binary" % (ctx.attr.name, ctx.attr.binary))

    output = ctx.actions.declare_file(ctx.label.name)

    if len(inputs) > 1:
        lipo.create(
            actions = ctx.actions,
            apple_fragment = ctx.fragments.apple,
            inputs = inputs,
            output = output,
            xcode_config = ctx.attr._xcode_config[apple_common.XcodeVersionConfig],
        )

    else:
        # If the transition doesn't split, this is building for a non-macOS
        # target, so just create a symbolic link of the input binary.
        ctx.actions.symlink(target_file = inputs[0], output = output)

    runfiles = ctx.runfiles(files = ctx.files.binary)
    transitive_runfiles = [
        binary[DefaultInfo].default_runfiles
        for binary in ctx.split_attr.binary.values()
    ]
    runfiles = runfiles.merge_all(transitive_runfiles)

    return [
        DefaultInfo(
            executable = output,
            files = depset([output]),
            runfiles = runfiles,
        ),
    ]

universal_binary = rule(
    attrs = dicts.add(
        apple_support.action_required_attrs(),
        {
            "binary": attr.label(
                cfg = macos_universal_transition,
                doc = "Target to generate a 'fat' binary from.",
                mandatory = True,
            ),
            "_allowlist_function_transition": attr.label(
                default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
            ),
        },
    ),
    doc = """
This rule produces a multi-architecture ("fat") binary targeting Apple macOS
platforms *regardless* of the architecture of the macOS host platform. The
`lipo` tool is used to combine built binaries of multiple architectures. For
non-macOS platforms, this simply just creates a symbolic link of the input
binary.
""",
    executable = True,
    fragments = ["apple"],
    implementation = _universal_binary_impl,
)
