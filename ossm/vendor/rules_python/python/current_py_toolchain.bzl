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

"""Public entry point for current_py_toolchain rule."""

load("//python/private:toolchain_types.bzl", "TARGET_TOOLCHAIN_TYPE")

def _current_py_toolchain_impl(ctx):
    toolchain = ctx.toolchains[ctx.attr._toolchain]

    direct = []
    transitive = []
    vars = {}

    if toolchain.py3_runtime and toolchain.py3_runtime.interpreter:
        direct.append(toolchain.py3_runtime.interpreter)
        transitive.append(toolchain.py3_runtime.files)
        vars["PYTHON3"] = toolchain.py3_runtime.interpreter.path

    if toolchain.py2_runtime and toolchain.py2_runtime.interpreter:
        direct.append(toolchain.py2_runtime.interpreter)
        transitive.append(toolchain.py2_runtime.files)
        vars["PYTHON2"] = toolchain.py2_runtime.interpreter.path

    files = depset(direct, transitive = transitive)
    return [
        toolchain,
        platform_common.TemplateVariableInfo(vars),
        DefaultInfo(
            runfiles = ctx.runfiles(transitive_files = files),
            files = files,
        ),
    ]

current_py_toolchain = rule(
    doc = """
    This rule exists so that the current python toolchain can be used in the `toolchains` attribute of
    other rules, such as genrule. It allows exposing a python toolchain after toolchain resolution has
    happened, to a rule which expects a concrete implementation of a toolchain, rather than a
    toolchain_type which could be resolved to that toolchain.
    """,
    implementation = _current_py_toolchain_impl,
    attrs = {
        "_toolchain": attr.string(default = str(TARGET_TOOLCHAIN_TYPE)),
    },
    toolchains = [
        str(TARGET_TOOLCHAIN_TYPE),
    ],
)
