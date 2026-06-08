# Copyright 2024 The Bazel Authors. All rights reserved.
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
"""Run a py_binary with altered config settings in an sh_test.

This facilitates verify running binaries with different configuration settings
without the overhead of a bazel-in-bazel integration test.
"""

load("@rules_shell//shell:sh_test.bzl", "sh_test")
load("//python/private:common_labels.bzl", "labels")  # buildifier: disable=bzl-visibility
load("//python/private:toolchain_types.bzl", "TARGET_TOOLCHAIN_TYPE")  # buildifier: disable=bzl-visibility
load(":py_reconfig.bzl", "py_reconfig_binary")

def sh_py_run_test(*, name, sh_src, py_src, **kwargs):
    """Run a py_binary within a sh_test.

    Args:
        name: name of the sh_test and base name of inner targets.
        sh_src: .sh file to run as a test
        py_src: .py file for the py_binary
        **kwargs: additional kwargs passed onto py_binary and/or sh_test
    """
    bin_name = "_{}_bin".format(name)
    sh_test(
        name = name,
        srcs = [sh_src],
        data = [bin_name],
        deps = [
            "@bazel_tools//tools/bash/runfiles",
        ],
        env = {
            "BIN_RLOCATION": "$(rlocationpaths {})".format(bin_name),
        },
    )
    py_reconfig_binary(
        name = bin_name,
        srcs = [py_src],
        main = py_src,
        tags = ["manual"],
        **kwargs
    )

def _current_build_settings_impl(ctx):
    info = ctx.actions.declare_file(ctx.label.name + ".json")
    toolchain = ctx.toolchains[TARGET_TOOLCHAIN_TYPE]
    runtime = toolchain.py3_runtime
    files = [info]
    ctx.actions.write(
        output = info,
        content = json.encode({
            "bootstrap_impl": ctx.attr._bootstrap_impl_flag[config_common.FeatureFlagInfo].value,
            "interpreter": {
                "short_path": runtime.interpreter.short_path if runtime.interpreter else None,
            },
            "interpreter_path": runtime.interpreter_path,
            "toolchain_label": str(getattr(toolchain, "toolchain_label", None)),
        }),
    )
    return [DefaultInfo(
        files = depset(files),
    )]

current_build_settings = rule(
    doc = """
Writes information about the current build config to JSON for testing.

This is so tests can verify information about the build config used for them.
""",
    implementation = _current_build_settings_impl,
    attrs = {
        "_bootstrap_impl_flag": attr.label(
            default = labels.BOOTSTRAP_IMPL,
        ),
    },
    toolchains = [
        TARGET_TOOLCHAIN_TYPE,
    ],
)
