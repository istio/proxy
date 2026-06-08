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

"""This module implements an alias rule to the resolved toolchain.
"""

load("//python/uv/private:toolchain_types.bzl", "UV_TOOLCHAIN_TYPE")

_DOC = """\
Exposes a concrete toolchain which is the result of Bazel resolving the
toolchain for the execution or target platform.
Workaround for https://github.com/bazelbuild/bazel/issues/14009
"""

# Forward all the providers
def _current_toolchain_impl(ctx):
    toolchain_info = ctx.toolchains[UV_TOOLCHAIN_TYPE]

    # Bazel requires executable rules to create the executable themselves,
    # so we create a symlink in this rule so that it appears this rule created its executable.
    original_uv_executable = toolchain_info.uv_toolchain_info.uv[DefaultInfo].files_to_run.executable

    # Use `uv` as the name of the binary to make the help message well formatted
    symlink_uv_executable = ctx.actions.declare_file("current_toolchain/uv".format(original_uv_executable.basename))
    ctx.actions.symlink(output = symlink_uv_executable, target_file = original_uv_executable)

    new_default_info = DefaultInfo(
        files = depset([symlink_uv_executable]),
        runfiles = toolchain_info.default_info.default_runfiles,
        executable = symlink_uv_executable,
    )

    template_variable_info = platform_common.TemplateVariableInfo({
        "UV_BIN": symlink_uv_executable.path,
    })

    return [
        toolchain_info,
        new_default_info,
        template_variable_info,
        toolchain_info.uv_toolchain_info,
    ]

# Copied from java_toolchain_alias
# https://cs.opensource.google/bazel/bazel/+/master:tools/jdk/java_toolchain_alias.bzl
current_toolchain = rule(
    implementation = _current_toolchain_impl,
    toolchains = [UV_TOOLCHAIN_TYPE],
    doc = _DOC,
    executable = True,
)
